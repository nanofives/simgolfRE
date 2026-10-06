// FLAGS golf_clean.exe: /O2 /GX
// golf_clean.exe batch s0 written by hand (release /O2, /GX for EH-framed constructors). Names are chosen here, not recovered.
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)

// Constructor of the class whose reset is 0x4a35e0 (golf_hand_r0.cpp): vbases at 0xe0 (ctor 0x4804a0) and 0x658 (ctor 0x489150), vtordisps at 0xdc and 0x654.
extern int g_839650;
#pragma vtordisp(off)
struct S0VB4a3110 { virtual void vb0(); ~S0VB4a3110(); int m_4; S0VB4a3110() { m_4 = g_839650; g_839650 = 0; } };
struct S0L4a3110 : virtual S0VB4a3110 {
    virtual void l0();
    virtual void vb0();
    int m_8, m_c, m_10, m_14, m_18;
    char padl[0x28 - 0x1c];
    S0L4a3110() { m_8 = 0; m_c = 0; m_10 = 0; m_14 = 0; m_18 = 0; }
};
struct S0LL4a3110 : S0L4a3110 {
    virtual void vb0();
    S0LL4a3110() {}
    ~S0LL4a3110();
};
#pragma vtordisp(on)
struct S0M4a3110 { S0M4a3110(); ~S0M4a3110(); char pad[0x60]; };   // ctor 0x492850
struct S0Q4a3110 {
    virtual void q0();
    void setA(int, int, int, int);    // 0x476310
    void setB(int, int, int, int);    // 0x476340
    void setC(int, int, int, int);    // 0x476370
    void setD(int, int, int, int);    // 0x4762d0
};
struct S0P4a3110 { virtual ~S0P4a3110(); char pad[0x270]; };
struct S0V1_4a3110 : S0P4a3110, S0Q4a3110 { S0V1_4a3110(); ~S0V1_4a3110(); char pad[0x574 - 0x278]; };   // ctor 0x4804a0
struct S0V2_4a3110 { virtual void r0(); S0V2_4a3110(); ~S0V2_4a3110(); char pad[0x100]; };              // ctor 0x489150
extern int g_8409cc;
extern int g_4e449c[4][3];
extern int g_83b628, g_83b62c, g_83b630;
struct S0C4a3110 : virtual S0V1_4a3110, virtual S0V2_4a3110 {
    int m_4;
    int m_8;
    int m_c;
    S0M4a3110 m_10;
    int m_70;
    int m_74;
    S0LL4a3110 m_78;
    int m_a8[13];
    virtual void r0();
    virtual void q0();
    ~S0C4a3110();
    S0C4a3110();
};
// MATCH: golf_clean.exe 0x004a3110 ??0S0C4a3110@@QAE@XZ
S0C4a3110::S0C4a3110()
{
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
    m_8 = g_8409cc;
    setA(g_4e449c[0][0], g_4e449c[1][0], g_4e449c[2][0], g_4e449c[3][0]);
    setB(g_4e449c[0][1], g_4e449c[1][1], g_4e449c[2][1], g_4e449c[3][1]);
    setC(g_4e449c[0][2], g_4e449c[1][2], g_4e449c[2][2], g_4e449c[3][2]);
    setD(g_83b628, g_83b62c, g_83b630, 0);
}

// Override of a virtual of the vbase at static offset 0x24 (vtordisp off): list row click, y/row height to selection index, callbacks at +0x10/+0x14.
struct S0Node49e230 { int m_0; int m_4; void* m_data; S0Node49e230* m_next; S0Node49e230* m_prev; };
struct S0Item49e230 { int m_0, m_4, m_8; };
struct S0List49e230 {
    char pad[8];
    S0Node49e230* m_head;
    S0Node49e230* m_cur;
    int m_count;
    int m_index;
    void seek(int i)
    {
        if (i > m_count - 1)
            return;
        m_cur = m_head;
        if (i < 0) {
            int n = abs(i);
            if (n > m_count)
                return;
            while (n > 0) {
                m_cur = m_cur->m_prev;
                n--;
            }
            i = m_count + i;
        } else {
            int n = i;
            while (n > 0) {
                m_cur = m_cur->m_next;
                n--;
            }
        }
        m_index = i;
    }
    void next()
    {
        if (m_head) {
            m_cur = m_cur->m_next;
            if (++m_index == m_count)
                m_index = 0;
        }
    }
    void* current() { return m_head ? m_cur->m_data : 0; }
};
struct S0Obj49e230 { VP16(a) VP16(b) VP16(c) VP4(d) virtual void d4(); virtual void notify(int); };
struct S0Q49e230 { int lineHeight(); };   // 0x477580
#pragma vtordisp(off)
struct S0V1_49e230 {
    virtual void onScroll(int, int);
    char pad[0x130 - 4];
    S0Obj49e230* m_130;
    char pad2[0x274 - 0x134];
    S0Q49e230 m_274;
    void f480ce0();   // 0x480ce0
};
struct S0V2_49e230 {
    virtual void w0();
    char pad[0x24 - 4];
    unsigned char m_24;
    char pad1[0x48 - 0x25];
    int m_48;
    char pad2[0x5c - 0x4c];
    int m_5c;
    char pad3[0x68 - 0x60];
    int m_68;
    int m_6c;
    char pad4[0xc0 - 0x70];
    S0List49e230 m_list;
    char pad5[0xf0 - 0xd8];
    int m_f0;
    int m_f4;
    int m_f8;
    void* getSel();   // 0x489950
};
struct S0D49e230 : virtual S0V1_49e230, virtual S0V2_49e230 {
    int m_4, m_8, m_c;
    void (*m_10)(int);
    void (*m_14)(void*);
    int m_18, m_1c, m_20;
    void onScroll(int y, int h);
    void mark(int i, int on);   // 0x49dab0
};
extern S0Obj49e230* g_83ab2c;
// MATCH: golf_clean.exe 0x0049e230 ?onScroll@S0D49e230@@UAEXHH@Z
void S0D49e230::onScroll(int y, int h)
{
    int first = 1;
    g_83ab2c = m_130;
    m_list.seek(-1);
    while (h > 0) {
        if (!first && m_list.m_index == m_list.m_count - 1)
            return;
        first = 0;
        S0List49e230& l = m_list;
        l.next();
        int w = 0;
        if (l.m_cur)
            w = ((S0Item49e230*)l.current())->m_8;
        int t = m_274.lineHeight() * w;
        h -= max(m_48 + m_6c, t);
    }
    int sel = y / (m_68 + m_48) * m_5c + m_list.m_index;
    if (m_f0 == sel) {
        if (m_14)
            m_14(getSel());
        if ((m_24 & 4) && m_130)
            m_130->notify(-1);
    } else {
        mark(m_f0, 0);
        m_f0 = sel;
        if (m_10)
            m_10(m_f0);
        mark(sel, 1);
        m_f4 = -1;
        m_f8 = -1;
        f480ce0();
        if ((m_24 & 4) && m_130)
            m_130->notify(-1);
    }
}
#pragma vtordisp(on)

// Status line: saves the text at 0x51a068, builds golfer name + number, copies the result to 0x5695a8 (priority byte 0x5a9cd0).
extern char g_5a9cd0;
extern char g_51a068[];
extern char g_5695a8[];
extern char g_58a528[];
extern char g_5794d9[][0x100];
extern const char s_004e9a84[];
extern const char s_004c4974[];
extern const char s_004c4970[];
void s0_appendGolferName4676e0(int, int);   // 0x4676e0
// MATCH: golf_clean.exe 0x00406d50 ?s0_status406d50@@YAXHH@Z
void s0_status406d50(int golfer, int prio)
{
    char buf[512];
    if (prio >= g_5a9cd0) {
        g_5a9cd0 = prio;
        if (golfer != -1) {
            strcpy(buf, g_51a068);
            strcpy(g_51a068, s_004e9a84);
            s0_appendGolferName4676e0(golfer, 0);
            strcat(g_51a068, s_004c4974);
            strcat(g_51a068, itoa(g_5794d9[golfer][0], g_58a528, 10));
            strcat(g_51a068, s_004c4970);
            strcpy(g_5695a8, g_51a068);
            strcat(g_5695a8, buf);
            strcpy(g_51a068, buf);
        } else
            strcpy(g_5695a8, g_51a068);
    }
}

// Scanline polygon fill: points/count/colour and clip rect into globals 0x83d34c..0x83d390, two edge walkers (0x492ed0 init, 0x492fa0 step), span 0x493080.


struct S0SurfI493100 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5();
    virtual void* bits();
    virtual void v7(); virtual void v8();
    virtual void unlock(int);
    virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15();
    VP16(a) VP16(b)
    virtual void w48(); virtual void w49(); virtual void w50();
    virtual RECT* clip();
    virtual void w52(); virtual void w53(); virtual void w54(); virtual void w55();
    virtual int pitch();
};
struct S0Surf493100 {
    int m_0;
    S0SurfI493100* m_4;
    void* bits() { return m_4 ? m_4->bits() : 0; }
    int pitch() { return m_4 ? m_4->pitch() : 0; }
    RECT* clip() { return m_4 ? m_4->clip() : 0; }
};
struct S0Edge493100 { int dir; int m_4; int m_8; int x; char pad[0x24 - 0x10]; };
extern void* g_83d380;
extern POINT* g_83d358;
extern int g_83d378;
extern int g_83d34c;
extern int g_83d388;
extern int g_83d35c, g_83d360, g_83d38c, g_83d390;
extern int g_83d350;
extern int g_83d384;
int s0_edgeInit492ed0(S0Edge493100* e, int i);   // 0x492ed0
int s0_edgeStep492fa0(S0Edge493100* e);          // 0x492fa0
void s0_span493080(int x1, int x2);               // 0x493080
// MATCH: golf_clean.exe 0x00493100 ?s0_fillPoly493100@@YAHPAUS0Surf493100@@PAUtagPOINT@@HH@Z
int s0_fillPoly493100(S0Surf493100* s, POINT* pts, int n, int color)
{
    if (!pts || !s)
        return 0x10;
    void* b = !s->m_4 ? 0 : s->m_4->bits();
    g_83d380 = b;
    if (b) {
        g_83d358 = pts;
        g_83d378 = n;
        g_83d34c = color;
        int p = s->pitch();
        g_83d388 = p;
        RECT* r = s->clip();
        if (r) {
            int ymax = -0x7ffd;
            int ymin = 0x7fff;
            int imin;
            int i;
            g_83d38c = r->right - 1;
            g_83d35c = r->left;
            g_83d390 = r->bottom - 1;
            g_83d360 = r->top;
            for (i = 0; i < n; i++) {
                if (pts[i].y < ymin) {
                    ymin = g_83d358[i].y;
                    imin = i;
                }
                if (pts[i].y > ymax) {
                    ymax = g_83d358[i].y;
                    g_83d350 = i;
                }
            }
            if (ymin < ymax) {
                S0Edge493100 l, rt;
                g_83d384 = ymin;
                l.dir = -1;
                if (s0_edgeInit492ed0(&l, imin)) {
                    rt.dir = 1;
                    if (s0_edgeInit492ed0(&rt, imin)) {
                        while (g_83d384 < g_83d390) {
                            if (g_83d384 >= g_83d360) {
                                if (l.x < rt.x)
                                    s0_span493080(l.x, rt.x);
                                else
                                    s0_span493080(rt.x, l.x);
                            }
                            if (!s0_edgeStep492fa0(&l) || !s0_edgeStep492fa0(&rt))
                                break;
                            g_83d384++;
                        }
                    }
                }
            }
        }
    }
    if (s->m_4)
        s->m_4->unlock(1);
    return 0;
}

// Save game: opens the save file named from the argument, writes the 100-byte header at 0x51a068 (date day = date%1024*30/1024+1), then the body serializer 0x40afa0.
extern char g_51a068[];
extern char g_58a528[];
extern int g_568d08;
extern int g_834170;
extern const char s_004c3f4c[];
extern const char s_004c3f44[];
extern const char s_004c52bc[];
extern const char s_004c52b8[];
extern const char s_004c3f70[];
void s0_f40daa0(int);   // 0x40daa0
void s0_f40d7b0(int);   // 0x40d7b0
void s0_save40afa0(int);   // 0x40afa0
// MATCH: golf_clean.exe 0x0040b4a0 ?s0_saveGame40b4a0@@YAXPBD@Z
void s0_saveGame40b4a0(const char* name)
{
    char buf[120];
    strcpy(buf, name);
    strcpy(g_51a068, s_004c3f4c);
    strcat(g_51a068, buf);
    strcat(g_51a068, s_004c3f44);
    g_568d08 = _open(g_51a068, 0x8301, 0x80);
    strcpy(g_51a068, s_004c52bc);
    s0_f40daa0(0);
    strcat(g_51a068, s_004c52b8);
    strcat(g_51a068, itoa(g_834170 % 1024 * 30 / 1024 + 1, g_58a528, 10));
    strcat(g_51a068, s_004c3f70);
    s0_f40d7b0(g_834170);
    _write(g_568d08, g_51a068, 100);
    s0_save40afa0(0);
    _close(g_568d08);
}

// Ten-slot picker loop: draws slots from 0x562914 (row = argument, scroll 0x5aa55c), arrows 10/11 scroll by 10, returns the clicked slot or -1/0.
struct S0Surf438390;
extern S0Surf438390* g_4c1570;
struct S0Img438390 {
    void draw(S0Surf438390* s, int x, int y, int f);                         // 0x473e60
    void draw2(S0Surf438390* s, int x, int y, int a, int b, int c, int d);   // 0x473cb0
    char pad[0x2c];
};
struct S0Slot438390 { S0Img438390 img; char pad[0x84 - 0x2c]; };
struct S0Mouse438390 { void get(int* x, int* y); void flip(int); };   // 0x47ab50, 0x480c80
extern S0Mouse438390 g_519a60;
extern S0Img438390 g_59e62c, g_59e658, g_59e684, g_59e6b0;
extern S0Img438390 g_59e708[2];
extern S0Slot438390 g_562914[];
extern POINT g_4c7be0[10];
extern int g_5aa55c;
extern int g_59b76c[];
extern int g_822d68;
int s0_hit4382f0(int x, int y);   // 0x4382f0
void s0_f45bf80(int, int);        // 0x45bf80
void s0_f45c030();                // 0x45c030
// MATCH: golf_clean.exe 0x00438390 ?s0_pick438390@@YAHH@Z
int s0_pick438390(int idx)
{
    for (;;) {
        int x, y;
        g_59e62c.draw(g_4c1570, 0, 0x102, 0);
        g_519a60.get(&x, &y);
        int hit = s0_hit4382f0(x, y);
        for (int i = 0; i < 10; i++) {
            g_59e708[hit == i].draw2(g_4c1570, g_4c7be0[i].x, g_4c7be0[i].y, 1, 1, 1, 0);
            if (g_5aa55c + i < g_59b76c[idx])
                g_562914[g_5aa55c + idx * 72 + i].img.draw2(g_4c1570, g_4c7be0[i].x - 8, g_4c7be0[i].y, 1, 1, 1, 0);
        }
        if (hit == 10)
            g_59e6b0.draw(g_4c1570, 0x2c3, 0x11c, 0);
        else if (hit == 11)
            g_59e658.draw(g_4c1570, 10, 0x1a9, 0);
        if (g_5aa55c == 0)
            g_59e684.draw(g_4c1570, 10, 0x1a9, 0);
        g_519a60.flip(0);
        s0_f45bf80(5, 0);
        s0_f45c030();
        if (g_822d68) {
            if (g_822d68 == 2 || y < 0x102)
                return -1;
            if (g_822d68 == 1) {
                if (hit != -1 && hit < 10)
                    return g_5aa55c + hit;
                if (hit == 10) {
                    if (g_5aa55c < 70)
                        g_5aa55c += 10;
                } else if (hit == 11 && g_5aa55c)
                    g_5aa55c -= 10;
                if (g_5aa55c > g_59b76c[idx] - 10)
                    g_5aa55c = g_59b76c[idx] - 10;
                if (g_5aa55c < 0)
                    g_5aa55c = 0;
            } else
                return 0;
        }
    }
}

// Landmark type name: long or short placeholder string per type 0..18 appended to 0x51a068.
extern char g_51a068[];
extern const char s_004c4c44[];
extern const char s_004c4c3c[];
extern const char s_004c4c28[];
extern const char s_004c4c20[];
extern const char s_004c4c00[];
extern const char s_004c4bec[];
extern const char s_004c4bd0[];
extern const char s_004c4bc4[];
extern const char s_004c4bac[];
extern const char s_004c4ba0[];
extern const char s_004c4b8c[];
extern const char s_004c4b80[];
extern const char s_004c4b60[];
extern const char s_004c4b4c[];
extern const char s_004c4b2c[];
extern const char s_004c4b20[];
extern const char s_004c4b0c[];
extern const char s_004c4b04[];
extern const char s_004c4af4[];
extern const char s_004c4ae8[];
extern const char s_004c4ad4[];
extern const char s_004c4ab4[];
extern const char s_004c4aa0[];
extern const char s_004c4a8c[];
extern const char s_004c4a84[];
extern const char s_004c4a64[];
extern const char s_004c4a50[];
extern const char s_004c4a38[];
extern const char s_004c4a28[];
extern const char s_004c4a10[];
extern const char s_004c4a00[];
extern const char s_004c49e8[];
extern const char s_004c49dc[];
extern const char s_004c49c0[];
extern const char s_004c49b0[];
extern const char s_004c4998[];
extern const char s_004c498c[];
extern const char s_004c4980[];
// MATCH: golf_clean.exe 0x004074a0 ?s0_landmarkName4074a0@@YAXHH@Z
void s0_landmarkName4074a0(int type, int full)
{
    const char* s;
    switch (type) {
    case 0:
        s = s_004c4c44;
        if (!full)
            s = s_004c4c3c;
        break;
    case 1:
        s = s_004c4c28;
        if (!full)
            s = s_004c4c20;
        break;
    case 2:
        s = s_004c4c00;
        if (!full)
            s = s_004c4bec;
        break;
    case 3:
        s = s_004c4bd0;
        if (!full)
            s = s_004c4bc4;
        break;
    case 4:
        s = s_004c4bac;
        if (!full)
            s = s_004c4ba0;
        break;
    case 5:
        s = s_004c4b8c;
        if (!full)
            s = s_004c4b80;
        break;
    case 6:
        s = s_004c4b60;
        if (!full)
            s = s_004c4b4c;
        break;
    case 7:
        s = s_004c4b2c;
        if (!full)
            s = s_004c4b20;
        break;
    case 8:
        s = s_004c4b0c;
        if (!full)
            s = s_004c4b04;
        break;
    case 9:
        s = s_004c4af4;
        if (!full)
            s = s_004c4ae8;
        break;
    case 10:
        s = s_004c4ad4;
        break;
    case 11:
        s = s_004c4ab4;
        if (!full)
            s = s_004c4aa0;
        break;
    case 12:
        s = s_004c4a8c;
        if (!full)
            s = s_004c4a84;
        break;
    case 13:
        s = s_004c4a64;
        if (!full)
            s = s_004c4a50;
        break;
    case 14:
        s = s_004c4a38;
        if (!full)
            s = s_004c4a28;
        break;
    case 15:
        s = s_004c4a10;
        if (!full)
            s = s_004c4a00;
        break;
    case 16:
        s = s_004c49e8;
        if (!full)
            s = s_004c49dc;
        break;
    case 17:
        s = s_004c49c0;
        if (!full)
            s = s_004c49b0;
        break;
    case 18:
        s = s_004c4998;
        if (!full)
            s = s_004c498c;
        break;
    default:
        s = s_004c4980;
    }
    strcat(g_51a068, s);
}

// 16-bit polygon fill: as 0x493100 plus palette translation of the colour through 0x83ad0c (format 0/1 table), span 0x493000.




struct S0Fmt4932d0 { int m_0; int m_4; };
struct S0SurfI4932d0 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5();
    virtual unsigned short* table0(); virtual unsigned short* table1();
    virtual void* bits();
    virtual void unlock(int);
    virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15();
    VP16(a) VP16(b)
    virtual void w48(); virtual void w49(); virtual void w50();
    virtual RECT* clip();
    virtual void w52(); virtual void w53(); virtual void w54(); virtual void w55();
    virtual int pitch();
    virtual S0Fmt4932d0* format();
};
struct S0Surf4932d0 {
    int m_0;
    S0SurfI4932d0* m_4;
    void* bits() { return m_4 ? m_4->bits() : 0; }
    int pitch() { return m_4 ? m_4->pitch() : 0; }
    RECT* clip() { return m_4 ? m_4->clip() : 0; }
    S0Fmt4932d0* format() { return m_4 ? m_4->format() : 0; }
    unsigned short* table0() { return m_4 ? m_4->table0() : 0; }
    unsigned short* table1() { return m_4 ? m_4->table1() : 0; }
};
struct S0Edge4932d0 { int dir; int m_4; int m_8; int x; char pad[0x24 - 0x10]; };
extern void* g_83d37c;
extern S0Surf4932d0* g_83ad0c;
extern POINT* g_83d358;
extern int g_83d378;
extern int g_83d34c;
extern int g_83d388;
extern int g_83d35c, g_83d360, g_83d38c, g_83d390;
extern int g_83d350;
extern int g_83d384;
int s0_edgeInit492ed0(S0Edge4932d0* e, int i);   // 0x492ed0
int s0_edgeStep492fa0(S0Edge4932d0* e);          // 0x492fa0
void s0_span493080(int x1, int x2);               // 0x493080
// MATCH: golf_clean.exe 0x004932d0 ?s0_fillPoly4932d0@@YAHPAUS0Surf4932d0@@PAUtagPOINT@@HI@Z
int s0_fillPoly4932d0(S0Surf4932d0* s, POINT* pts, int n, unsigned int color)
{
    if (!pts || !s)
        return 0x10;
    void* b = !s->m_4 ? 0 : s->m_4->bits();
    g_83d37c = b;
    if (b) {
        S0Surf4932d0* pal;
        if (!(color & 0x80000000) && (pal = g_83ad0c) != 0) {
            unsigned short* t;
            switch (s->format()->m_4) {
            case 0:
                t = pal->table0();
                break;
            case 1:
                t = pal->table1();
                break;
            default:
                return 1;
            }
            color = t[color & 0xff];
        }
        g_83d358 = pts;
        g_83d34c = color & 0xffff;
        g_83d378 = n;
        int p = s->pitch();
        g_83d388 = p * 2;
        RECT* r = s->clip();
        if (r) {
            int ymax = -0x7ffd;
            int ymin = 0x7fff;
            int imin;
            int i;
            g_83d38c = r->right - 1;
            g_83d35c = r->left;
            g_83d390 = r->bottom - 1;
            g_83d360 = r->top;
            for (i = 0; i < n; i++) {
                if (pts[i].y < ymin) {
                    ymin = g_83d358[i].y;
                    imin = i;
                }
                if (pts[i].y > ymax) {
                    ymax = g_83d358[i].y;
                    g_83d350 = i;
                }
            }
            if (ymin < ymax) {
                S0Edge4932d0 l, rt;
                g_83d384 = ymin;
                l.dir = -1;
                if (s0_edgeInit492ed0(&l, imin)) {
                    rt.dir = 1;
                    if (s0_edgeInit492ed0(&rt, imin)) {
                        while (g_83d384 < g_83d390) {
                            if (g_83d384 >= g_83d360) {
                                if (l.x < rt.x)
                                    s0_span493080(l.x, rt.x);
                                else
                                    s0_span493080(rt.x, l.x);
                            }
                            if (!s0_edgeStep492fa0(&l) || !s0_edgeStep492fa0(&rt))
                                break;
                            g_83d384++;
                        }
                    }
                }
            }
        }
    }
    if (s->m_4)
        s->m_4->unlock(1);
    return 0;
}

// Rebuilds the 50x50 Terrain.dll tiles from the game grids: type/variation, corner raises, normals, walls, paths.
struct S0Tile449540 { char pad[0x248]; };
struct S0Terrain449540 {
    char pad[0x14];
    int m_width;
    int m_height;
    char pad2[0x3a4 - 0x1c];
    S0Tile449540 m_tiles[1];
    S0Tile449540* tileAt(int x, int y)
    {
        if (x >= m_width || x < 0 || y >= m_height || y < 0)
            return 0;
        return &m_tiles[x + y * m_width];
    }
    void setType(S0Tile449540*, int, int);           // Terrain.dll import
    void elevateCorner(S0Tile449540*, int);          // Terrain.dll import
    void calcNormals(S0Tile449540*);                 // Terrain.dll import
    void setWall(S0Tile449540*, int, int, int);      // Terrain.dll import
    void layPath(S0Tile449540*, int, int);           // Terrain.dll import
};
extern S0Terrain449540* g_820ed0;
struct S0Cell449540 { unsigned char m_0; char m_1; };
extern S0Cell449540 g_53caf0[50][50];
void s0_f483bd0();                    // 0x483bd0
int s0_type4492d0(int, int);          // 0x4492d0
int s0_var4492f0(int, int);           // 0x4492f0
int s0_corner449310(int, int, int);   // 0x449310
int s0_wall449330(int, int, int);     // 0x449330
int s0_path4493b0(int, int);          // 0x4493b0
// MATCH: golf_clean.exe 0x00449540 ?s0_rebuildTerrain449540@@YAXXZ
void s0_rebuildTerrain449540()
{
    int x, y, c, i;
    for (x = 49; x >= 0; x--) {
        s0_f483bd0();
        for (y = 49; y >= 0; y--)
            g_820ed0->setType(g_820ed0->tileAt(x, y), s0_type4492d0(x, y), s0_var4492f0(x, y));
    }
    for (x = 49; x >= 0; x--) {
        s0_f483bd0();
        for (y = 49; y >= 0; y--) {
            if (s0_type4492d0(x, y) != 0x11) {
                for (c = 1; c < 8; c += 2) {
                    if (s0_corner449310(x, y, c) > 3) {
                        for (i = 0; i < s0_corner449310(x, y, c) - 3; i++)
                            g_820ed0->elevateCorner(g_820ed0->tileAt(x, y), c);
                    }
                }
            }
        }
    }
    for (x = 49; x >= 0; x--)
        for (y = 49; y >= 0; y--)
            g_820ed0->calcNormals(g_820ed0->tileAt(x, y));
    for (x = 49; x >= 0; x--)
        for (y = 49; y >= 0; y--)
            for (c = 0; c < 8; c += 2)
                if (s0_wall449330(x, y, c))
                    g_820ed0->setWall(g_820ed0->tileAt(x, y), c, s0_wall449330(x, y, c), 1);
    for (x = 49; x >= 0; x--)
        for (y = 49; y >= 0; y--)
            if (s0_path4493b0(x, y))
                g_820ed0->layPath(g_820ed0->tileAt(x, y), s0_path4493b0(x, y), g_53caf0[x][y].m_0 & 0x40);
}
