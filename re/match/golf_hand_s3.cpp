// FLAGS golf_clean.exe: /O2 /GX
// golf_clean.exe batch s3 (phase 6, functions over 1 KB) written by hand (release /O2). Names are chosen here, not recovered.
#include <string.h>
#pragma warning(disable: 4715)
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>

extern int g_567afc, g_4c2848, g_53df54, g_4c2854, g_822d68, g_59b734;
extern char g_51a068[];
extern const char s_004c7d80[], s_004c7d00[], s_004c7cac[], s_004d6098[], s_004c7c48[], s_004c5f18[], s_004c5bf8[];
int S3f4326a0(int, int);
int S3f46d6e0(int, int, int, int, int);
void S3f455a30();
void S3f473470();
int S3f437910(int, int, int);
void S3f40cb00(unsigned int, int, int);
void S3f40daa0(int);
void S3f45b2c0(const char*, int, int, int);
int S3f405ac0(char*);
void S3f45b8b0(int);
void S3f4481b0(int, int, int, int, int);
void S3f432560();
// MATCH: golf_clean.exe 0x00432720 ?S3menu432720@@YAHHH@Z
int S3menu432720(int x, int y)
{
    int r;
    switch (S3f4326a0(x, y)) {
    case 0:
        g_567afc = (g_567afc == 0) ? 5 : 0;
        g_4c2848 = -1;
        g_53df54 = 0;
        g_4c2854 = -1;
        return -1;
    case 1:
        g_567afc = (g_567afc == 1) ? 5 : 1;
        g_4c2848 = -1;
        g_53df54 = 0;
        g_4c2854 = -1;
        return -1;
    case 2:
        g_567afc = (g_567afc == 2) ? 5 : 2;
        g_4c2848 = -1;
        g_53df54 = 0;
        g_4c2854 = -1;
        return -1;
    case 3:
        if (g_822d68 == 2)
            return -112;
        strcpy(g_51a068, s_004c7d80);
        strcat(g_51a068, s_004c7d00);
        switch (S3f46d6e0(200, 0xfa, 1, 1, 0)) {
        case 0: return 0x3f;
        case 1: return -112;
        case 2: return -113;
        case 3: return 0x52;
        case 4: return -114;
        case 5: return -118;
        case 6: return -115;
        case 7: return -120;
        case 8: return -121;
        case 9: return 0x57;
        case 10: S3f455a30(); return -1;
        case 11: S3f473470(); return -1;
        }
        return -1;
    case 4:
        return 0x50;
    case 5:
        strcpy(g_51a068, s_004c7cac);
        strcat(g_51a068, s_004d6098);
        strcat(g_51a068, s_004c7c48);
        r = S3f46d6e0(0xfa, 0x154, 1, 1, 0);
        for (;;) {
            switch (r) {
            case 0: return 0x53;
            case 1: return 0x4c;
            case 2: return 0x43;
            case 3:
                if (S3f437910(0, 1, 1)) {
                    strcpy(g_51a068, s_004d6098);
                    strcat(g_51a068, s_004c5f18);
                    S3f40cb00(0x80006000, 0, -1);
                    return -1;
                }
                strcpy(g_51a068, s_004c7cac);
                strcat(g_51a068, s_004d6098);
                strcat(g_51a068, s_004c7c48);
                r = S3f46d6e0(0xfa, 0x154, 1, 1, 0);
                break;
            case 4:
                g_51a068[0] = 0;
                S3f40daa0(-1);
                S3f45b2c0(s_004c5bf8, 200, 0x14c, 0x20);
                if (g_51a068[0] && S3f405ac0(g_51a068)) {
                    S3f45b8b0(0);
                    return -1;
                }
                S3f4481b0(0x18, 100, 0, 0, 0);
                return -1;
            case 5: S3f432560(); return -1;
            case 6: return 0x6e;
            case 7: g_59b734 = 1; return 0x1b;
            default: return -1;
            }
        }
    case 6: return 0x3f;
    case 7: return 0x7a;
    case 8: return 0x78;
    case 9: return -36;
    case 10: return -33;
    default:
        return -1;
    }
}


struct S3Dev44b9c0 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9();
    virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19();
    virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29();
    virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39();
    virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49();
    virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual int width(); virtual int height();
};
struct S3Surf {
    S3Surf();                                                   // 0x474ae0
    ~S3Surf();                                                  // 0x474c40
    virtual void b();
    S3Dev44b9c0* m_4;
    char pad[0x2b8 - 8];
    int create(int, int, int, int, int, int);                   // 0x474dd0
    int load(const char*, void*, int, int, int);                // 0x475840
    void release();                                             // 0x474cb0
    void f4762d0(struct S3Font44b9c0*, int, int, int);                           // 0x4762d0
    void f476310(int, int, int, int);                           // 0x476310
    void text(const char*, int, int, int);                      // 0x477c30
    int width() { return m_4 ? m_4->width() : 0; }
    int height() { return m_4 ? m_4->height() : 0; }
};
struct S3ScrA { virtual void a(); char pad[0x270]; };
struct S3Scr : S3ScrA, S3Surf { virtual void a(); virtual void b(); void f480c80(int); };   // 0x480c80
extern S3Scr g_519a60;
struct S3Spr {
    S3Spr();                                                    // 0x473ab0
    ~S3Spr() { release(); }                                     // inline copy of 0x4041f0
    virtual void v();
    void release();                                             // 0x473ae0
    int make(S3Surf*, int, int, int, int, int, int);            // 0x473c60
    void blit(S3Surf*, int, int, int);                          // 0x473e60
    int draw(S3Surf*, int, int, S3Surf*);                       // 0x473df0
    int m_4, m_8, m_c, m_10, m_14, m_18, m_1c, m_20, m_24, m_28;
};
extern const char s_004d21cc[], s_004d21c0[], s_004d21b8[], s_004d21ac[], s_004d21a4[];
extern char* g_83b164;
struct S3Font44b9c0 { int m_0, m_4, m_8, m_c, m_10; };
extern S3Font44b9c0 g_821020;
extern int g_822d68;
int S3f487e70(const char*, const char*);
char* S3f487e90();
void S3f487e60();
void S3f483d30();
void S3f45c030();
int S3f45ae70();
void S3f45bf80(int, int);
// MATCH: golf_clean.exe 0x0044b9c0 ?S3credits44b9c0@@YAXXZ
void S3credits44b9c0()
{
    S3Spr back;
    S3Spr logo;
    S3Surf surf;
    char* lines[512];
    int count = 0;
    bool run = true;
    int total;
    DWORD t0;
    int lineH;
    int i;
    surf.create(800, 600, 0, 1, 0, 0);
    surf.load(s_004d21cc, 0, 0, 0x100, 1);
    back.make(&surf, 0, 0, 800, 600, 1, 0);
    surf.load(s_004d21c0, 0, 0, 0x100, 1);
    logo.make(&surf, 0, 0, surf.width(), surf.height(), 1, 1);
    surf.release();
    if (!S3f487e70(s_004d21ac, s_004d21b8)) {
        while (*S3f487e90() != '#') {
            lines[count] = (char*)malloc(strlen(g_83b164) + 1);
            if (lines[count])
                strcpy(lines[count++], g_83b164);
        }
        S3f487e60();
        g_519a60.f4762d0(&g_821020, 0, 0, 0);
        g_519a60.f476310(0x80007fff, -1, 2, 2);
        t0 = timeGetTime();
        lineH = (g_821020.m_8 >= 0) ? g_821020.m_8 + g_821020.m_10 : g_821020.m_c;
        S3f483d30();
        if (count) {

            do {
                int y = 600 - (timeGetTime() - t0) / 30;
                back.blit(&g_519a60, 0, 0, 0);
                i = 0;
                if (-y > lineH * count + 150)
                    run = false;
                for (; y < 600 && i < count; ) {
                    char* s = lines[i];
                    if (*s && !strcmp(s, s_004d21a4))
                        logo.draw(&g_519a60, (800 - logo.m_10) / 2, y, 0);
                    else if (s)
                        g_519a60.text(s, 50, y, strlen(s));
                    y += lineH;
                    i++;
                    S3f45c030();
                }
                if (g_822d68 || S3f45ae70())
                    run = false;
                g_519a60.f480c80(0);
                S3f45bf80(1, 0);
            } while (run);
            while (count) {
                count--;
                if (lines[count])
                    free(lines[count]);
            }
        }
    }
    back.release();
    logo.release();
}


#pragma vtordisp(off)
extern const char s_004c1434[];
extern const char s_004c1458[];
// operator new(size_t, S3Heap*) is the inline form of 0x4823c0; the `else p = 0` shape is what lets VC6 skip the
// new-expression null re-check on the malloc path.
struct S3Heap { void* alloc(size_t); };   // 0x474860
extern S3Heap* g_839650;
struct S3HeapV {
    S3HeapV() { m_4 = g_839650; g_839650 = 0; }
    virtual ~S3HeapV();
    static void* operator new(size_t n, S3Heap* h)
    {
        void* p;
        if (h) {
            p = h->alloc(n);
            if (p) {
                g_839650 = h;
                return p;
            }
            MessageBoxA(0, s_004c1434, s_004c1458, 0);
            _exit(3);
        }
        p = malloc(n);
        if (p)
            g_839650 = 0;
        else
            p = 0;
        return p;
    }
    static void operator delete(void*, S3Heap*);
    static void operator delete(void*);
    S3Heap* m_4;
};
struct S3Data401d10 : virtual S3HeapV { int m_4, m_8, m_c; };
struct S3Node401d10 : virtual S3HeapV { int m_4; S3Data401d10* m_data; S3Node401d10* m_next; S3Node401d10* m_prev; };
struct S3List401d10 : virtual S3HeapV {
    virtual void onAdd(S3Data401d10*);
    S3Node401d10* m_head;
    S3Node401d10* m_cur;
    int m_count, m_14;
    S3Heap* m_18;
    int add(int key);
};
// MATCH: golf_clean.exe 0x00401d10 ?add@S3List401d10@@QAEHH@Z
int S3List401d10::add(int key)
{
    if (m_count == 0) {
        if (m_18)
            m_head = new(m_18) S3Node401d10;
        else
            m_head = new(S3HeapV::m_4) S3Node401d10;
        if (!m_head)
            return 4;
        m_head->m_4 = key;
        m_head->m_next = m_head;
        m_head->m_prev = m_head;
        if (m_18)
            m_head->m_data = new(m_18) S3Data401d10;
        else
            m_head->m_data = new(S3HeapV::m_4) S3Data401d10;
        if (!m_head->m_data)
            return 4;
        onAdd(m_head->m_data);
        m_cur = m_head;
        m_14 = m_count++;
        return 0;
    }
    if (m_18)
        m_head->m_prev->m_next = new(m_18) S3Node401d10;
    else
        m_head->m_prev->m_next = new(S3HeapV::m_4) S3Node401d10;
    if (!m_head->m_prev->m_next)
        return 4;
    m_head->m_prev->m_next->m_prev = m_head->m_prev;
    m_head->m_prev->m_next->m_next = m_head;
    m_head->m_prev = m_head->m_prev->m_next;
    m_cur = m_head->m_prev;
    m_cur->m_4 = key;
    if (m_18)
        m_cur->m_data = new(m_18) S3Data401d10;
    else
        m_cur->m_data = new(S3HeapV::m_4) S3Data401d10;
    if (!m_cur->m_data)
        return 4;
    onAdd(m_cur->m_data);
    m_14 = m_count++;
    return 0;
}
// Same list as 0x401d10 with a 0x10-byte data object.
struct S3Data402280 : virtual S3HeapV { int m_4; };
struct S3Node402280 : virtual S3HeapV { int m_4; S3Data402280* m_data; S3Node402280* m_next; S3Node402280* m_prev; };
struct S3List402280 : virtual S3HeapV {
    virtual void onAdd(S3Data402280*);
    S3Node402280* m_head;
    S3Node402280* m_cur;
    int m_count, m_14;
    S3Heap* m_18;
    int add(int key);
};
// MATCH: golf_clean.exe 0x00402280 ?add@S3List402280@@QAEHH@Z
int S3List402280::add(int key)
{
    if (m_count == 0) {
        if (m_18)
            m_head = new(m_18) S3Node402280;
        else
            m_head = new(S3HeapV::m_4) S3Node402280;
        if (!m_head)
            return 4;
        m_head->m_4 = key;
        m_head->m_next = m_head;
        m_head->m_prev = m_head;
        if (m_18)
            m_head->m_data = new(m_18) S3Data402280;
        else
            m_head->m_data = new(S3HeapV::m_4) S3Data402280;
        if (!m_head->m_data)
            return 4;
        onAdd(m_head->m_data);
        m_cur = m_head;
        m_14 = m_count++;
        return 0;
    }
    if (m_18)
        m_head->m_prev->m_next = new(m_18) S3Node402280;
    else
        m_head->m_prev->m_next = new(S3HeapV::m_4) S3Node402280;
    if (!m_head->m_prev->m_next)
        return 4;
    m_head->m_prev->m_next->m_prev = m_head->m_prev;
    m_head->m_prev->m_next->m_next = m_head;
    m_head->m_prev = m_head->m_prev->m_next;
    m_cur = m_head->m_prev;
    m_cur->m_4 = key;
    if (m_18)
        m_cur->m_data = new(m_18) S3Data402280;
    else
        m_cur->m_data = new(S3HeapV::m_4) S3Data402280;
    if (!m_cur->m_data)
        return 4;
    onAdd(m_cur->m_data);
    m_14 = m_count++;
    return 0;
}
#pragma vtordisp(on)


// ==== 0x004017d0 (S3animal4017d0) ====
struct S3Obj4017d0 {
    short m_0, m_2, m_4, m_6, m_8, m_a, m_c, m_e;
    int m_10, m_14;
    char m_18, m_19, m_1a;
    char pad[0x3c - 0x1b];
};
extern S3Obj4017d0 g_56d1d8[];
struct S3Ent4017d0 { short m_0, m_2, m_4, m_6; int m_8, m_c; };
extern S3Ent4017d0 g_58bcb8[256];
struct S3Map4017d0 { char cell[4][6]; char pad[0x74 - 24]; };
extern S3Map4017d0 g_4e6d70[];
struct S3Rng4017d0 { unsigned short roll(int); };   // 0x45c1e0
extern S3Rng4017d0 g_822d9c;
extern unsigned int g_59e7b8;
extern int g_53f3e8[];
extern int g_5685f4;
extern int g_4e9a80;
extern int g_4c2878[8];
extern int g_4c2898[8];
extern char g_51a068[];
extern char g_4e9a70[];
extern const char s_004c1430[];
int S3f42fb90(int, int, int*, int*, int);
int S3f43d6f0(int, int, int);
void S3f462a30(int, int, int, int, int, int, int, int);
void S3f40c500(int, int, int, int);
// MATCH: golf_clean.exe 0x004017d0 ?S3animal4017d0@@YAXH@Z
void S3animal4017d0(int idx)
{
    int sx, sy, n, tx, ty;
    int i, r, d;
    int wx = (g_56d1d8[idx].m_6 << 10) + g_56d1d8[idx].m_10;
    int wy = (g_56d1d8[idx].m_8 << 10) + g_56d1d8[idx].m_14;
    int sprite = g_56d1d8[idx].m_c + g_56d1d8[idx].m_0;
    if (S3f42fb90(wx, wy, &sx, &sy, 0)) {
        if (g_56d1d8[idx].m_a < 0x80) {
        n = 0;
        for (i = 0; i < 256; i++)
            if (g_58bcb8[i].m_0 == 5 && g_58bcb8[i].m_8 > 0 && g_58bcb8[i].m_2 == g_56d1d8[idx].m_6 && g_58bcb8[i].m_4 == g_56d1d8[idx].m_8)
                n = g_58bcb8[i].m_8 - 1;
        d = (g_5685f4 - g_56d1d8[idx].m_18 - 2) & 7;
        g_56d1d8[idx].m_1a = g_56d1d8[idx].m_1a % g_53f3e8[sprite];
        g_4e9a80 = S3f43d6f0(sprite, g_56d1d8[idx].m_1a, d);
        S3f462a30(sx, sy, sy, 0x1e, 0x28, g_4e9a80, 4, (n + 0x9c) | 0x100);
        if (!(g_59e7b8 & 4)) {
            if (++g_56d1d8[idx].m_1a >= g_53f3e8[sprite])
                g_56d1d8[idx].m_1a = 0;
        }
        g_51a068[0] = 0;
        strcat(g_51a068, itoa((g_56d1d8[idx].m_10 << 1) >> 10, g_4e9a70, 10));
        strcat(g_51a068, s_004c1430);
        strcat(g_51a068, itoa((g_56d1d8[idx].m_14 << 1) >> 10, g_4e9a70, 10));
        }
    } else {
        if (!(g_59e7b8 & 4) && !g_822d9c.roll(500))
            g_56d1d8[idx].m_a = 500;
    }
    if (g_56d1d8[idx].m_a) {
        g_56d1d8[idx].m_c = 0x140;
        if (!(g_59e7b8 & 4))
            g_56d1d8[idx].m_a--;
        return;
    }
    if (g_56d1d8[idx].m_1a == 0 && g_56d1d8[idx].m_c != 0x15a && g_56d1d8[idx].m_c != 0x140) {
        g_56d1d8[idx].m_c = 0x140;
        g_56d1d8[idx].m_a = g_822d9c.roll(0x40) + 0x40;
    }
    if (g_56d1d8[idx].m_19 == 0) {
        if (g_56d1d8[idx].m_c == 0x140 && !g_822d9c.roll(8)) {
            tx = (g_56d1d8[idx].m_10 << 1) >> 10;
            ty = (g_56d1d8[idx].m_14 << 1) >> 10;
            r = g_822d9c.roll(3);
            g_56d1d8[idx].m_18 = (g_56d1d8[idx].m_18 + r - 1) & 7;
            tx += g_4c2878[g_56d1d8[idx].m_18];
            ty += g_4c2898[g_56d1d8[idx].m_18];
            if (tx < 0 || ty < 0 || tx >= 4 || ty >= 4) {
                int lim = (g_59e7b8 & 0x200000) ? 0x40 : 0x18;
                if (!g_822d9c.roll(lim)) {
                    S3f40c500(g_56d1d8[idx].m_0 + 0x82, wx, wy, 0);
                    g_56d1d8[idx].m_c = 0x14d;
                    g_56d1d8[idx].m_1a = 0;
                }
            } else if (g_4e6d70[g_56d1d8[idx].m_2].cell[tx][ty] == -1) {
                g_56d1d8[idx].m_19 = 0x10;
            }
        }
        if (g_56d1d8[idx].m_19 == 0)
            return;
    }
    g_56d1d8[idx].m_c = 0x15a;
    g_56d1d8[idx].m_10 += g_4c2878[g_56d1d8[idx].m_18] * 1024 / 32;
    g_56d1d8[idx].m_14 += g_4c2898[g_56d1d8[idx].m_18] * 1024 / 32;
    if (--g_56d1d8[idx].m_19 <= 0)
        g_56d1d8[idx].m_c = 0x140;
}
