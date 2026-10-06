// FLAGS golf_clean.exe: /O2 /GX
// golf_clean.exe batch s1 written by hand (release /O2, /GX). Names are chosen here.
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#pragma vtordisp(off)
// 0x00489150: constructor; member at +0xc0 has a virtual base whose inline ctor takes the 0x839650 global.
struct S1T3 { int a, b, c; };
struct S1T4 { int a, b, c, d; };
extern int g_4e44cc, g_4e4488, g_4e448c, g_4e4490, g_4e4494, g_4e4498;
extern S1T3 g_4e449c[4];
extern int g_83b61c, g_83b620, g_83b624;
extern S1T3 g_83b628;
extern S1T4 g_83b634;
extern int g_839650;
struct S1V489150 {
    S1V489150() { m_4 = g_839650; g_839650 = 0; }
    virtual ~S1V489150();
    int m_4;
};
struct S1VB489150 { virtual ~S1VB489150(); };
struct S1Data489150 : virtual S1VB489150 { };
struct S1Node489150 : virtual S1VB489150 { int m_4; S1Data489150* m_data; S1Node489150* m_next; };
// List; the clear loop is inline in reset().
struct S1B489150 : virtual S1V489150 {
    S1B489150() { m_head = 0; m_cur = 0; m_count = 0; m_14 = 0; m_18 = 0; }
    virtual void f();
    virtual void onRemove(S1Data489150*);
    virtual ~S1B489150();
    void removeAll()
    {
        int i;
        S1Data489150* d;
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
    S1Node489150* m_head;
    S1Node489150* m_cur;
    int m_count, m_14, m_18;
};
struct S1C489150 : S1B489150 {
    virtual void f();
    virtual ~S1C489150();
    int m_1c, m_20, m_24;
};
struct S1M4747a0 { S1M4747a0(); virtual ~S1M4747a0(); void clear(); void reset() { clear(); m_18 = 0; } char pad[0x14]; int m_18; };   // clear 0x4747e0
struct S1D489150 {
    S1D489150();
    void reset();
    virtual ~S1D489150();
    S1M4747a0 m_4;
    int m_20, m_24, m_28, m_2c, m_30, m_34, m_38, m_3c, m_40, m_44, m_48, m_4c;
    int m_50, m_54;
    int m_58, m_5c, m_60, m_64, m_68, m_6c, m_70;
    S1T3 m_74;
    S1T3 m_80[4];
    S1T4 m_b0;
    S1C489150 m_c0;
    int m_f0, m_f4;
};
// MATCH: golf_clean.exe 0x00489150 ??0S1D489150@@QAE@XZ
S1D489150::S1D489150()
{
    m_24 = g_4e44cc;
    m_28 = 0;
    m_2c = 0;
    m_30 = g_4e4488;
    m_34 = g_83b61c;
    m_f0 = 0;
    m_f4 = -1;
    m_3c = 0;
    m_40 = 0;
    m_44 = g_83b624;
    m_48 = g_4e448c;
    m_38 = g_83b620;
    m_4c = -1;
    m_50 = g_4e4490;
    m_54 = g_4e4494;
    m_58 = 0;
    m_5c = 0;
    m_60 = 0;
    m_64 = 0;
    m_68 = 0;
    m_6c = 0;
    m_70 = g_4e4498;
    m_b0.a = g_83b634.a;
    m_b0.b = g_83b634.b;
    m_b0.c = g_83b634.c;
    m_b0.d = g_83b634.d;
    m_74.a = g_83b628.a;
    m_80[0].a = g_4e449c[0].a;
    m_80[1].a = g_4e449c[1].a;
    m_80[2].a = g_4e449c[2].a;
    m_80[3].a = g_4e449c[3].a;
    m_74.b = g_83b628.b;
    m_80[0].b = g_4e449c[0].b;
    m_80[1].b = g_4e449c[1].b;
    m_80[2].b = g_4e449c[2].b;
    m_80[3].b = g_4e449c[3].b;
    m_74.c = g_83b628.c;
    m_80[0].c = g_4e449c[0].c;
    m_80[1].c = g_4e449c[1].c;
    m_80[2].c = g_4e449c[2].c;
    m_80[3].c = g_4e449c[3].c;
    m_20 = -1;
}
// 0x004894b0: reset; clears the list member and restores the constructor defaults.
// MATCH: golf_clean.exe 0x004894b0 ?reset@S1D489150@@QAEXXZ
void S1D489150::reset()
{
    m_c0.reset();
    m_4.clear();
    m_4.m_18 = 0;
    m_24 = g_4e44cc;
    m_28 = 0;
    m_2c = 0;
    m_30 = g_4e4488;
    m_34 = g_83b61c;
    m_f0 = 0;
    m_f4 = -1;
    m_3c = 0;
    m_40 = 0;
    m_44 = g_83b624;
    m_48 = g_4e448c;
    m_4c = -1;
    m_38 = g_83b620;
    m_50 = g_4e4490;
    m_54 = g_4e4494;
    m_58 = 0;
    m_5c = 0;
    m_60 = 0;
    m_64 = 0;
    m_68 = 0;
    m_6c = 0;
    m_70 = g_4e4498;
    m_b0.a = g_83b634.a;
    m_b0.b = g_83b634.b;
    m_b0.c = g_83b634.c;
    m_b0.d = g_83b634.d;
    m_74.a = g_83b628.a;
    m_80[0].a = g_4e449c[0].a;
    m_80[1].a = g_4e449c[1].a;
    m_80[2].a = g_4e449c[2].a;
    m_80[3].a = g_4e449c[3].a;
    m_74.b = g_83b628.b;
    m_80[0].b = g_4e449c[0].b;
    m_80[1].b = g_4e449c[1].b;
    m_80[2].b = g_4e449c[2].b;
    m_80[3].b = g_4e449c[3].b;
    m_74.c = g_83b628.c;
    m_80[0].c = g_4e449c[0].c;
    m_80[1].c = g_4e449c[1].c;
    m_80[2].c = g_4e449c[2].c;
    m_80[3].c = g_4e449c[3].c;
    m_20 = -1;
}
// 0x00431fa0: writes the course summary text file (name with the 0x4c7928 extension).
#include <stdio.h>
#include <string.h>
extern const char s_004c7928[], s_004c7924[], s_004c7918[], s_004c790c[], s_004c7900[], s_004c78f8[], s_004c78ec[];
extern const char s_004c78e0[], s_004c78d4[], s_004c78c4[], s_004c78b0[], s_004c78a4[], s_004c7894[], s_004c7880[];
extern const char s_004c786c[], s_004c7858[], s_004c7844[];
extern char g_51a068[];
extern int g_5685f0, g_59aafc, g_58d36c, g_56a524, g_571fd4, g_59ae78, g_541cd8, g_5a882c, g_56949c, g_5a636c;
extern unsigned char g_5a34e0;
extern const char* g_4c3078[];
extern char g_567328[], g_4d6098[];
extern const double g_4ba488;
void __cdecl S1fn40daa0(int);
void __cdecl S1fn45b9f0(int);
// MATCH: golf_clean.exe 0x00431fa0 ?S1writeCourseInfo@@YAXPAD@Z
void S1writeCourseInfo(char* name)
{
    char* p = strchr(name, '.');
    if (p)
        *p = 0;
    strcat(name, s_004c7928);
    FILE* f = fopen(name, s_004c7924);
    fprintf(f, s_004c7918);
    g_51a068[0] = 0;
    S1fn40daa0(1);
    fprintf(f, s_004c790c, g_51a068);
    fprintf(f, s_004c7900, g_5685f0 - 1);
    fprintf(f, s_004c78f8, g_59aafc);
    fprintf(f, s_004c78ec, g_58d36c);
    fprintf(f, s_004c78e0, g_4c3078[g_5a34e0 & 3]);
    fprintf(f, s_004c78d4, g_567328);
    fprintf(f, s_004c78c4, g_4d6098);
    g_51a068[0] = 0;
    S1fn45b9f0(0x14);
    fprintf(f, s_004c78b0, g_56a524, g_51a068);
    fprintf(f, s_004c78a4, g_571fd4 * 100);
    fprintf(f, s_004c7894, g_59ae78);
    fprintf(f, s_004c7880, g_541cd8 * g_4ba488);
    fprintf(f, s_004c786c, g_5a882c * g_4ba488);
    fprintf(f, s_004c7858, g_56949c * g_4ba488);
    fprintf(f, s_004c7844, g_5a636c * g_4ba488);
    fclose(f);
}
// 0x00483420: nearest palette index to (r,g,b); with reserved!=0 it skips indices outside 10..0xf5 and those claimed by the 5 ranges.
#include <string.h>
struct S1Pal483420 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void getEntries(unsigned char* out, int first, int count); };
struct S1Range483420 { int m_id; int m_4; unsigned char m_start; unsigned char m_count; char pad[6]; };
struct S1C483420 {
    int m_0;
    S1Pal483420* m_pal;
    S1Range483420 m_ranges[5];
    int nearest(unsigned char r, unsigned char g, unsigned char b, int reserved);
};
// MATCH: golf_clean.exe 0x00483420 ?nearest@S1C483420@@QAEHEEEH@Z
int S1C483420::nearest(unsigned char r, unsigned char g, unsigned char b, int reserved)
{
    int best = 200000;
    int bestIdx;
    unsigned char pal[0x300];
    int used[0x100];
    int i, d;
    if (!m_pal)
        return 7;
    m_pal->getEntries(pal, 0, 0x100);
    if (!reserved) {
        for (i = 0; i < 0x100; i++) {
            d = (pal[i * 3 + 2] - b) * (pal[i * 3 + 2] - b) + (pal[i * 3] - r) * (pal[i * 3] - r) + (pal[i * 3 + 1] - g) * (pal[i * 3 + 1] - g);
            if (d < best) {
                best = d;
                bestIdx = i;
            }
        }
    } else {
        memset(used, 0, sizeof(used));
        for (i = 0; i < 5; i++) {
            if (m_ranges[i].m_id != -1) {
                for (int j = m_ranges[i].m_start; j < m_ranges[i].m_start + m_ranges[i].m_count; j++)
                    used[j] = 1;
            }
        }
        for (i = 10; i < 0xf6; i++) {
            if (!used[i]) {
                d = (pal[i * 3 + 2] - b) * (pal[i * 3 + 2] - b) + (pal[i * 3] - r) * (pal[i * 3] - r) + (pal[i * 3 + 1] - g) * (pal[i * 3 + 1] - g);
                if (d < best) {
                    best = d;
                    bestIdx = i;
                }
            }
        }
    }
    return bestIdx;
}
// 0x0042ef40: rates tile (x,y): border sum of 0x42ee80 times the best per-record score from the 18-entry table at 0x575cb8, divided by 40.
int __cdecl S1tileValue42ee80(int x, int y);
int __cdecl S1dist40acd0(int dx, int dy);
struct S1Rec575cb8 {
    char m_active; char pad1[7];
    int m_ax, m_ay; char pad2[8];
    int m_bx, m_by;
    int m_n, m_m; char pad3[0x130];
    short m_158; char pad4[0xa6];
    int m_flags; char pad5[4];
};
extern S1Rec575cb8 g_575cb8[18];
struct S1Pos59aea8 { int x, y; char pad[0x10]; };
extern S1Pos59aea8 g_59aea8[18];
extern int g_822c88, g_543cd0;
// MATCH: golf_clean.exe 0x0042ef40 ?S1rate42ef40@@YAHHH@Z
int S1rate42ef40(int x, int y)
{
    int sum = 0;
    int i;
    for (i = -1; i < 3; i++) {
        sum += S1tileValue42ee80(x + i, y - 1);
        sum += S1tileValue42ee80(x + i, y + 2);
    }
    for (i = 0; i < 2; i++) {
        sum += S1tileValue42ee80(x - 1, y + i);
        sum += S1tileValue42ee80(x + 2, y + i);
    }
    int best = 0;
    for (i = 0; i < 18; i++) {
        S1Rec575cb8* r = &g_575cb8[i];
        if (r->m_active && r->m_n) {
            int v = r->m_158 * 1000 / (r->m_n + r->m_m / 2 + 4) + (3 - g_822c88) * 100;
            if (r->m_flags & 1)
                v += 100;
            if (r->m_flags & 2)
                v += 100;
            int d = S1dist40acd0(x - r->m_ax, y - r->m_ay);
            int d2 = S1dist40acd0(x - r->m_bx, y - r->m_by);
            if (d2 < d)
                d = d2;
            if (g_59aea8[i].x != -1) {
                d2 = S1dist40acd0(x - (g_59aea8[i].x >> 10), y - (g_59aea8[i].y >> 10));
                if (d2 < d)
                    d = d2;
            }
            v = v / (d + 8);
            if (v > best)
                best = v;
        }
    }
    if (g_543cd0)
        best += g_543cd0 * best / 3;
    return best * sum / 40;
}
// 0x0047f700: window hit test; the four captured pointers 0x83ab40/44/30/34 are dropped while their bit 0 at +0xa0 is clear.
struct S1W47f700 {
    char pad[0xa0];
    unsigned char m_a0; char pad2[0xf];
    S1W47f700* m_b0;
    void toLocal(int* x, int* y);     // 0x47b200
    void toLocal2(int* x, int* y);    // 0x47b2d0
};
S1W47f700* __cdecl S1hit47f340(S1W47f700* w, int* x, int* y);
extern int g_839ab8, g_839abc, g_83ab18, g_83ab94;
extern S1W47f700* g_83ab40;
extern S1W47f700* g_83ab44;
extern S1W47f700* g_83ab30;
extern S1W47f700* g_83ab34;
extern S1W47f700* g_83ab54;
extern S1W47f700* g_83ab50;
extern S1W47f700* g_83a2d8[];
// MATCH: golf_clean.exe 0x0047f700 ?S1hitTest47f700@@YAPAUS1W47f700@@PAH0@Z
S1W47f700* S1hitTest47f700(int* x, int* y)
{
    S1W47f700* r;
    int i;
    g_839ab8 = *x;
    g_839abc = *y;
    r = 0;
again:
    if (g_83ab40) {
        if (!(g_83ab40->m_a0 & 1)) {
            g_83ab40 = 0;
            goto again;
        }
        g_83ab40->toLocal(x, y);
        g_83ab18 = 0;
        r = g_83ab40;
    } else if (g_83ab44) {
        if (!(g_83ab44->m_a0 & 1)) {
            g_83ab44 = 0;
            goto again;
        }
        g_83ab44->toLocal2(x, y);
        g_83ab18 = 1;
        r = g_83ab44;
    } else if (g_83ab30) {
        if (!(g_83ab30->m_a0 & 1)) {
            g_83ab30 = 0;
            goto again;
        }
        g_83ab30->toLocal(x, y);
        g_83ab18 = 0;
        r = g_83ab30;
    } else if (g_83ab34) {
        if (!(g_83ab34->m_a0 & 1)) {
            g_83ab34 = 0;
            goto again;
        }
        g_83ab34->toLocal2(x, y);
        g_83ab18 = 1;
        r = g_83ab34;
    }
    if (r)
        return r;
    if (g_83ab54) {
        g_83ab54->toLocal2(x, y);
        r = S1hit47f340(g_83ab54, x, y);
        if (!r) {
            if (g_83ab50) {
                *x = g_839ab8;
                *y = g_839abc;
                g_83ab50->toLocal2(x, y);
                r = S1hit47f340(g_83ab50, x, y);
            }
            if (!r) {
                *x = g_839ab8;
                *y = g_839abc;
                g_83ab54->toLocal(x, y);
                r = g_83ab54;
                g_83ab18 = 0;
            }
        }
    } else {
        for (i = 0; i < g_83ab94; i++) {
            r = S1hit47f340(g_83a2d8[i], x, y);
            if (r)
                break;
        }
    }
    if (r && !g_83ab18)
        r = r->m_b0;
    return r;
}
// 0x00434140: building pick at (x,y); -3/-2/-1 from 0x4340a0 are undo, mode switch and nothing; otherwise selects building r+6 unless it is locked.
#include <string.h>
extern int g_55e928, g_567afc, g_4c2e08, g_4c2854, g_5685f0, g_5a6364, g_5a9f64;
extern int g_5a8c38[];   // 0x5a8c50 is g_5a8c38[6]
extern char g_51a068[];
extern const char s_004c7ec4[], s_004c7e7c[], s_004c80d0[], s_004c8084[], s_004c803c[];
int __cdecl S1fn433d30(int, int);
int __cdecl S1fn4340a0(int, int);
void __cdecl S1fn40d320(int, int, int, int);
void __cdecl S1fn45c0c0(int);
int __cdecl S1fn44faf0(int);
void __cdecl S1fn4481b0(int, int, int, int, int);
void __cdecl S1fn40cb00(int, int, int);
struct S1Obj519a60 { void f480c80(int); };
extern S1Obj519a60 g_519a60;
// MATCH: golf_clean.exe 0x00434140 ?S1pickBuilding434140@@YAHHH@Z
int S1pickBuilding434140(int x, int y)
{
    if (g_55e928)
        return S1fn433d30(x, y);
    int r = S1fn4340a0(x, y);
    if (r == -3) {
        strcpy(g_51a068, s_004c7ec4);
        g_4c2e08 = -22;
        strcat(g_51a068, s_004c7e7c);
        S1fn40d320(200, 0x154, 0x80007fff, -2);
        g_519a60.f480c80(0);
        S1fn45c0c0(0);
        return 0;
    }
    if (r == -2) {
        g_55e928 = 1;
        g_567afc = 1;
        return 0;
    }
    if (r == -1)
        return -1;
    g_4c2854 = r + 6;
    if ((g_5a8c38[g_4c2854] && S1fn44faf0(g_5685f0 - 1) < 2) || g_5a8c38[g_4c2854] >= 2) {
        S1fn4481b0(0x18, 100, 0, 0, 0);
        if (g_5a8c38[g_4c2854] >= 2)
            strcpy(g_51a068, s_004c80d0);
        else
            strcpy(g_51a068, s_004c8084);
        S1fn40cb00(0x80006000, 1, -1);
        g_4c2854 = -1;
    }
    if (g_4c2854 >= g_5a6364) {
        S1fn4481b0(0x18, 100, 0, 0, 0);
        strcpy(g_51a068, s_004c803c);
        S1fn40cb00(0x80006000, 1, -1);
        g_4c2854 = -1;
    }
    return g_5a9f64;
}
// 0x00433e50: draws the tool palette and, after 11 frames over the same tool, its tooltip.
#include <string.h>
struct S1Font433e50 { void f4762d0(void* p, int a, int b, int c); };
extern S1Font433e50* g_4c1570;
struct S1Spr433e50 { void draw(S1Font433e50* f, int x, int y, int flags); char pad[0x2c]; };   // 0x473e60
struct S1Tool433e50 { S1Spr433e50 m_icon; S1Spr433e50 m_lit; S1Spr433e50 m_58; };
extern S1Tool433e50 g_571d40[];
extern S1Spr433e50 g_58b2a8;
struct S1Panel433e50 { void draw(void* img, S1Font433e50* f, int x, int y, int flags); };   // 0x473f60
extern S1Panel433e50 g_541b78;
extern char g_58b4f0[], g_519fd8[];
struct S1Pos433e50 { short x, y; };
extern S1Pos433e50 g_4c7a88[];
extern int g_59e7b8, g_542f20, g_5a9f54, g_5aa554;
extern char g_51a068[];
extern const char s_004c7f2c[], s_004c8030[], s_004c801c[], s_004c8008[], s_004c7ff4[], s_004c7fe0[];
int __cdecl S1toolAt433c60(int x, int y);
void __cdecl S1tip432620(int x, int y);
// MATCH: golf_clean.exe 0x00433e50 ?S1drawTools433e50@@YAXHH@Z
void S1drawTools433e50(int x, int y)
{
    int r = S1toolAt433c60(x, y);
    g_541b78.draw(g_58b4f0, g_4c1570, 0xd7, 0x1e2, 0);
    g_4c1570->f4762d0(g_519fd8, 0, 0, 0);
    if (r == -2)
        g_58b2a8.draw(g_4c1570, 0xfe, 0x1e3, 0);
    if (g_59e7b8 & 0x8000000)
        g_571d40[4].m_lit.draw(g_4c1570, 0xfd, 0x222, 0);
    if (r == -3)
        g_571d40[4].m_icon.draw(g_4c1570, 0xfd, 0x222, 0);
    else if (r >= 0)
        g_571d40[r].m_icon.draw(g_4c1570, g_4c7a88[r].x, g_4c7a88[r].y, 0);
    g_571d40[g_542f20].m_lit.draw(g_4c1570, g_4c7a88[g_542f20].x, g_4c7a88[g_542f20].y, 0);
    if (r == g_5a9f54) {
        g_5aa554++;
        if (r != -1 && g_5aa554 > 10) {
            g_51a068[0] = 0;
            switch (r) {
            case -3: strcpy(g_51a068, s_004c7f2c); break;
            case -2: strcpy(g_51a068, s_004c8030); break;
            case 0: strcpy(g_51a068, s_004c801c); break;
            case 1: strcpy(g_51a068, s_004c8008); break;
            case 2: strcpy(g_51a068, s_004c7ff4); break;
            case 3: strcpy(g_51a068, s_004c7fe0); break;
            }
            S1tip432620(x, y - 5);
        }
    } else {
        g_5aa554 = 0;
        g_5a9f54 = r;
    }
}
// 0x00436060: button bar click/draw; 0x435f00 picks the button, cases 4-8 set g_58f330.
struct S1Spr436060 { void draw(S1Font433e50* f, int x, int y, int flags); };   // 0x473e60
extern S1Spr436060 g_583b88, g_583c38, g_583ce8;
extern short g_4c7ac4, g_4c7ac6, g_4c7ac8, g_4c7aca, g_4c7acc, g_4c7ace;
extern int g_561254, g_5685f0, g_59e7b8, g_4c2e14, g_5a59f8, g_4d60b4, g_58f330, g_567afc, g_59ca54;
extern char g_582cd0, g_582cd1;
extern short g_582d6e;
int __cdecl S1fn436c00(int, int);
int __cdecl S1fn435f00(int, int);
void __cdecl S1fn4385d0(int, int);
// MATCH: golf_clean.exe 0x00436060 ?S1click436060@@YAHHH@Z
int S1click436060(int x, int y)
{
    if (g_561254)
        return S1fn436c00(x, y);
    int b = S1fn435f00(x, y);
    if (g_5685f0 <= 1 || (g_59e7b8 & 0x4200000))
        g_583b88.draw(g_4c1570, g_4c7ac4, g_4c7ac6, 0);
    if (!(g_59e7b8 & 0x2000) || g_5685f0 <= 1 || g_4c2e14 == -1 || (g_59e7b8 & 0x4200000))
        g_583c38.draw(g_4c1570, g_4c7ac8, g_4c7aca, 0);
    if (!(g_59e7b8 & 0x400000) || (g_59e7b8 & 0x4200000))
        g_583ce8.draw(g_4c1570, g_4c7acc, g_4c7ace, 0);
    switch (b) {
    case 0:
        g_582cd0 = 0x20;
        g_582d6e = 0;
        if (g_4d60b4)
            g_582cd1 = g_4d60b4 & 7;
        else
            g_582cd1 = 7;
        S1fn4385d0(0, 0x98);
        return -1;
    case 1:
        if (g_5685f0 > 1 && !(g_59e7b8 & 0x4200000) && g_5a59f8 == -1) {
            g_59e7b8 = (g_59e7b8 & ~0x2000) | 0x4000;
            return -1;
        }
        break;
    case 2:
        if ((g_59e7b8 & 0x2000) && g_5685f0 > 1 && g_4c2e14 != -1 && g_5a59f8 == -1 && !(g_59e7b8 & 0x4200000)) {
            g_59e7b8 |= 0x4000;
            return -1;
        }
        break;
    case 3:
        if ((g_59e7b8 & 0x400000) && !(g_59e7b8 & 0x4200000) && g_5a59f8 == -1)
            return 0x4a;
        break;
    case 4:
        g_58f330 = 0;
        return -1;
    case 5:
        g_58f330 = -1;
        return -1;
    case 6:
        g_58f330 = 1;
        return -1;
    case 7:
        g_58f330 = 3;
        return -1;
    case 8:
        g_58f330 = 4;
        return -1;
    case 9:
        g_567afc = 2;
        return -1;
    case 10:
        g_59ca54 = 1;
        g_561254 = 1;
        break;
    }
    return -1;
}
// 0x00487fb0: text reader: opens the file (default extension 0x4e4418) and seeks to the line '#'+section.
#include <stdio.h>
#include <string.h>
extern const char s_004d3884[], s_004e4418[], s_004e4438[], s_004e442c[];
extern char g_83c004[];
int __cdecl S1findSection488420(char* file, char* section);
void __cdecl S1log4a0320(const char* fmt, ...);
void __cdecl S1trim4925d0(char* s);
void __cdecl S1trim4925b0(char* s);
struct S1Reader487fb0 {
    int m_0;
    char m_name[0x50];
    char m_54[0x100];
    char* m_cur;
    FILE* m_file;
    char* m_line;
    void close();   // 0x487f80
    int open(char* name, char* section);
};
// MATCH: golf_clean.exe 0x00487fb0 ?open@S1Reader487fb0@@QAEHPAD0@Z
int S1Reader487fb0::open(char* name, char* section)
{
    int again = 0;
    if (name && _strnicmp(name, m_name, strlen(name))) {
        strcpy(m_name, name);
        if (!strchr(m_name, '.'))
            strcat(m_name, s_004e4418);
        close();
        m_file = fopen(m_name, s_004d3884);
        if (!m_file)
            return 1;
        strcpy(m_54, g_83c004);
    } else if (m_file) {
        again = 1;
    } else {
        m_file = fopen(m_name, s_004d3884);
        if (!m_file)
            return 1;
        strcpy(m_54, g_83c004);
    }
    if (section) {
        char key[0x50];
        strcpy(key, s_004e4438);
        strcat(key, section);
        int off = S1findSection488420(m_name, section);
        if (off >= 0) {
            S1log4a0320(s_004e442c, section, off, 0, 0);
            fseek(m_file, off, 0);
            again = 1;
        }
        for (;;) {
            if (feof(m_file)) {
                if (!again) {
                    close();
                    return 1;
                }
                again = 0;
                rewind(m_file);
            }
            if (!fgets(m_line, 0x1ff, m_file) && !feof(m_file)) {
                close();
                return 1;
            }
            if (!feof(m_file)) {
                S1trim4925d0(m_line);
                S1trim4925b0(m_line);
            }
            if (!_stricmp(key, m_line))
                break;
        }
        m_cur = m_line;
    }
    return 0;
}
// 0x0048db60: view init; reuses the list type of 0x489150 (S1C489150) for the members at 0x13e0 and 0x1410.
struct S1Surf48db60 { int create(int p); char pad[0xe3c]; };   // 0x474820
struct S1List48db60 : S1C489150 {
    void clear4026a0();
    void attach(S1Surf48db60* s) { clear4026a0(); m_14 = 0; m_18 = (int)s; }
    void detach() { reset(); m_18 = 0; }
};
struct S1VA48db60 { virtual ~S1VA48db60(); int m_4; };
struct S1VF48db60 {
    virtual ~S1VF48db60();
    void clearBits() { m_flags &= ~3; }
    void setBits() { m_flags |= 3; }
    char pad[0x20];
    unsigned int m_flags;
};
struct S1Obj48db60 : virtual S1VA48db60, virtual S1VF48db60 {
    virtual void g();
    void attach(S1Surf48db60* s);   // 0x4a12e0
    void detach(int);               // 0x4a1250
};
struct S1Ctl48db60 { void f492920(int n); char pad[0x64]; };
struct S1View48db60 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62(); virtual void v63(); virtual void v64(); virtual void v65(); virtual void v66(); virtual void v67(); virtual void v68(); virtual void v69(); virtual void v70(); virtual void v71(); virtual void v72(); virtual void v73(); virtual void v74(); virtual void v75(); virtual void v76(); virtual void v77(); virtual void v78(); virtual void v79(); virtual void v80(); virtual void v81(); virtual void v82(); virtual void v83(); virtual void v84(); virtual void v85(); virtual void v86(); virtual void v87(); virtual void v88(); virtual void v89(); virtual void v90(); virtual void v91(); virtual void v92(); virtual void v93(); virtual void v94(); virtual void v95(); virtual void v96(); virtual void v97(); virtual void v98(); virtual void v99(); virtual void v100(); virtual void v101(); virtual void v102(); virtual void v103(); virtual void v104();
    void f48d480();
    void f48e190(int a, int b);
    void f48e120(int a);
    void f48e0b0(int a);
    int init(unsigned int flags, int src);
    char pad4[0x58c];
    int m_590, m_594, m_598, m_59c, m_5a0;
    S1Surf48db60 m_5a4;
    S1List48db60 m_13e0;
    S1List48db60 m_1410;
    char pad1420[0x20];
    S1Obj48db60 m_1460;
    char pad1498[0x1bc];
    int m_1654;
    char pad1658[0x8ac];
    S1Ctl48db60 m_1f04;
    unsigned int m_1f68;
};
// MATCH: golf_clean.exe 0x0048db60 ?init@S1View48db60@@QAEHIH@Z
int S1View48db60::init(unsigned int flags, int src)
{
    f48d480();
    if (!(flags & 0x100000))
        m_590 = 0;
    if (flags & 1)
        m_1654 = 1;
    else if (flags & 2)
        m_1654 = 2;
    else if (flags & 4)
        m_1654 = 4;
    else
        m_1654 = 0x10;
    if (src) {
        if (m_5a4.create(src))
            return 4;
        if (!(flags & 0x40000)) {
            m_1410.attach(&m_5a4);
            m_13e0.attach(&m_5a4);
            m_1460.attach(&m_5a4);
            goto done;
        }
    }
    m_1410.detach();
    m_13e0.detach();
    m_1460.detach(0);
done:
    m_1f68 = flags;
    if (!m_590) {
        m_1460.clearBits();
        f48e190(0x1000, 0x1000);
    } else {
        f48e120(m_59c);
        f48e0b0(m_5a0);
        f48e190(m_594, m_598);
        m_1460.setBits();
    }
    if (flags & 0x1000)
        f48e190(0x1000, 0x1000);
    m_1f04.f492920(10);
    v104();
    return 0;
}
