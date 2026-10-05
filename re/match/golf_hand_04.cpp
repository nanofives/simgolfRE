// FLAGS golf_clean.exe: /O2 /GX
// golf_clean.exe batch 04 written by hand (release /O2, /GX for the EH-framed destructors and new[]): list seek helpers,
// destructors, virtual-base overrides, string setters and small game-table loops. Names are chosen here.
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
struct Node489 { int m_0; void* m_data; int m_8; Node489* m_next; Node489* m_prev; };
struct List489 {
    Node489* m_head;
    Node489* m_cur;
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
    void* get(int i)
    {
        seek(i);
        if (m_head)
            return m_cur->m_data;
        return 0;
    }
};
struct C489a30 { char pad[0xc8]; List489 m_list; void* get(int i); };
// MATCH: golf_clean.exe 0x00489a30 ?get@C489a30@@QAEPAXH@Z
void* C489a30::get(int i)
{
    return m_list.get(i);
}
struct C4a48f0 { char pad[0x80]; List489 m_list; void* get(int i); };
// MATCH: golf_clean.exe 0x004a48f0 ?get@C4a48f0@@QAEPAXH@Z
void* C4a48f0::get(int i)
{
    return m_list.get(i);
}
struct C489950 { char pad[0xc8]; List489 m_list; char pad2[0xf0 - 0xd8]; int m_f0; void* getSel(); };
// MATCH: golf_clean.exe 0x00489950 ?getSel@C489950@@QAEPAXXZ
void* C489950::getSel()
{
    return m_list.get(m_f0);
}
struct Slot572 { int m_x, m_y, m_8; short m_c, m_e; char m_10, m_11; unsigned char m_12; char m_13; };
extern Slot572 g_572cb0[128];
struct Rng822d9c { int roll(int); };
extern Rng822d9c g_822d9c;
// MATCH: golf_clean.exe 0x00405970 ?addSlot405970@@YAHHHD@Z
int addSlot405970(int x, int y, char c)
{
    int i;
    for (i = 0; i < 128; i++) {
        if (g_572cb0[i].m_12 == 0xff) {
            g_572cb0[i].m_x = (x << 10) + 0x200;
            g_572cb0[i].m_y = (y << 10) + 0x200;
            g_572cb0[i].m_11 = g_822d9c.roll(4);
            g_572cb0[i].m_e = 0x10c;
            g_572cb0[i].m_8 = -20;
            g_572cb0[i].m_12 = c;
            g_572cb0[i].m_c = -1;
            return i;
        }
    }
    return -1;
}
struct N480410 { int m_0, m_4; void* m_8; N480410* m_c; };
struct C480410 {
    virtual ~C480410()
    {
        if (m_4) {
            if (!m_14) {
                for (int i = 0; i < m_c; i++) {
                    m_8 = m_4->m_c;
                    if (m_4->m_8)
                        free(m_4->m_8);
                    m_4->m_8 = 0;
                    if (m_4)
                        free(m_4);
                    m_4 = m_8;
                }
            }
            m_4 = 0;
            m_10 = 0;
            m_c = 0;
        }
        m_10 = 0;
    }
    N480410* m_4;
    N480410* m_8;
    int m_c, m_10, m_14;
};
C480410* new480410() { return new C480410; }
// MATCH: golf_clean.exe 0x00480410 ??_GC480410@@UAEPAXI@Z
extern "C" {
__declspec(dllimport) void __stdcall BinkDoFrame(void*);
__declspec(dllimport) int __stdcall BinkBufferLock(void*);
__declspec(dllimport) int __stdcall BinkCopyToBuffer(void*, void*, int, int, int, int, int);
__declspec(dllimport) int __stdcall BinkBufferUnlock(void*);
__declspec(dllimport) int __stdcall BinkGetRects(void*, int);
__declspec(dllimport) void __stdcall BinkBufferBlit(void*, void*, int);
__declspec(dllimport) void __stdcall BinkNextFrame(void*);
}
struct Bink487 { int m_0, m_4; unsigned m_frames, m_frame; char pad[0x34 - 0x10]; char m_rects[4]; };
struct Buf487 { int m_0, m_height, m_8, m_c, m_type; void* m_buffer; int m_pitch; };
struct C487180 { int m_0; Bink487* m_bink; Buf487* m_buf; bool frame(); };
// MATCH: golf_clean.exe 0x00487180 ?frame@C487180@@QAE_NXZ
bool C487180::frame()
{
    BinkDoFrame(m_bink);
    if (BinkBufferLock(m_buf)) {
        BinkCopyToBuffer(m_bink, m_buf->m_buffer, m_buf->m_pitch, m_buf->m_height, 0, 0, m_buf->m_type);
        BinkBufferUnlock(m_buf);
    }
    BinkBufferBlit(m_buf, m_bink->m_rects, BinkGetRects(m_bink, m_buf->m_type));
    if (m_bink->m_frame == m_bink->m_frames)
        return false;
    BinkNextFrame(m_bink);
    return true;
}
struct I4745 { virtual void v0(); virtual int create(int, int, int, int, int, int); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void setAlpha(int); };
struct B4745 { I4745* m_surf; };
struct Mgr4745 {
    virtual void v00(); virtual void v01(); virtual void v02(); virtual void v03(); virtual void v04(); virtual void v05(); virtual void v06(); virtual void v07();
    virtual void v08(); virtual void v09(); virtual void v0a(); virtual void v0b(); virtual void v0c(); virtual void v0d(); virtual void v0e(); virtual void v0f();
    virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17();
    virtual void v18(); virtual void v19(); virtual void v1a(); virtual void v1b(); virtual void v1c(); virtual void v1d(); virtual void v1e(); virtual void v1f();
    virtual I4745* make(B4745*, int);
};
extern Mgr4745* g_83ad50b;
extern int g_83acb4;
int* get492450();
struct C4745c0 : B4745 {
    virtual void v();
    void reset();      // 0x473ae0
    int init(int x, int y);
    char pad[0x10 - 8];
    int m_10, m_14, m_18, m_1c, m_20, m_24;
};
// MATCH: golf_clean.exe 0x004745c0 ?init@C4745c0@@QAEHHH@Z
int C4745c0::init(int x, int y)
{
    reset();
    m_surf = g_83ad50b->make(this, 1);
    m_surf->setAlpha(0xff);
    if (m_surf->create(0, x, y, *get492450(), 1, g_83acb4)) {
        m_10 = x;
        m_14 = y;
        m_18 = x;
        m_1c = y;
        m_20 = 0;
        m_24 = 0;
    }
    return 0;
}
struct E492 { E492(); virtual ~E492(); char pad[0x1c]; };
struct C492920 {
    void release();    // 0x492830
    void alloc(int n);
    char pad[0x50];
    E492* m_50;
    int m_54, m_58;
};
// MATCH: golf_clean.exe 0x00492920 ?alloc@C492920@@QAEXH@Z
void C492920::alloc(int n)
{
    release();
    m_50 = new E492[n];
    if (m_50) {
        m_54 = n;
        m_58 = 0;
    }
}
int call484110(int);
struct B4852d0 { virtual ~B4852d0(); char pad[0x38]; unsigned m_3c; };
struct C4841e0 : B4852d0 {
    virtual ~C4841e0();
    void cleanup();      // 0x4844e0
    int m_40;
    char pad2[0x50 - 0x44];
    void* m_50;
};
// MATCH: golf_clean.exe 0x004841e0 ??1C4841e0@@UAE@XZ
C4841e0::~C4841e0()
{
    if (m_50) {
        free(m_50);
        m_50 = 0;
    }
    if (m_40) {
        call484110(m_40);
        m_40 = 0;
    }
    cleanup();
}
struct Mgr83ad80 { void release(void*); };   // 0x4876c0
extern Mgr83ad80 g_83ad80c;
struct C4848a0 : C4841e0 {
    virtual ~C4848a0();
};
// MATCH: golf_clean.exe 0x004848a0 ??1C4848a0@@UAE@XZ
C4848a0::~C4848a0()
{
    if (m_3c < 16)
        g_83ad80c.release(this);
    if (m_50)
        free(m_50);
    m_50 = 0;
    cleanup();
}
struct B4805a0 { virtual ~B4805a0(); char pad[0x270]; };          // dtor 0x4805a0
struct B49c274 { virtual void v(); };
struct M49c574 { virtual void v(); void shutdown(); ~M49c574() { shutdown(); } char pad[0x20]; };   // shutdown 0x4838b0
// The 0x598 call (0x486ce0) is made from the body: a member the compiler destroys would add an EH state the original lacks.
struct M49c598 { void release(); int m_0; };                        // 0x486ce0
struct C49c780 : B4805a0, B49c274 {
    virtual ~C49c780();
    void cleanup();      // 0x49c0a0
    char pad2[0x574 - 0x278];
    M49c574 m_574;
    M49c598 m_598;
};
// MATCH: golf_clean.exe 0x0049c780 ??1C49c780@@UAE@XZ
C49c780::~C49c780()
{
    cleanup();
    m_598.release();
}
struct C488a20 { char pad[0x5f8]; char* m_5f8; int setText(const char* s); };
// MATCH: golf_clean.exe 0x00488a20 ?setText@C488a20@@QAEHPBD@Z
int C488a20::setText(const char* s)
{
    if (m_5f8) {
        free(m_5f8);
        m_5f8 = 0;
    }
    if (s) {
        m_5f8 = (char*)malloc(strlen(s) + 1);
        if (!m_5f8)
            return 4;
        *m_5f8 = 0;
        strcat(m_5f8, s);
    }
    return 0;
}
struct V04a1 { int m_0; };
struct V4a1 { int handle(int); int handle2(int); };                                   // 0x4896b0, 0x4897f0
struct P4a10 : virtual V04a1, virtual V4a1 {
    virtual void v();
    void prep0();                                                                     // 0x489e40
    int run0(int a) { prep0(); return handle(a); }
    int run0b(int a) { prep0(); return handle2(a); }
    char pad[0x50];
};
struct P4a158 : virtual V04a1, virtual V4a1 {
    void prep58();                                                                    // 0x49d690
    int run58(int a) { prep58(); return handle(a); }
    int run58b(int a) { prep58(); return handle2(a); }
    char pad[0x1c];
};
struct P4a178 : virtual V04a1, virtual V4a1 {
    void prep78();                                                                    // 0x49ece0
    int run78(int a) { prep78(); return handle(a); }
    int run78b(int a) { prep78(); return handle2(a); }
    char pad[0x14];
};
struct P4a190 { int handle90(int); int handle90b(int); char pad[0xdc]; };            // 0x4a3790, 0x4a3860
struct P4a116c : virtual V04a1, virtual V4a1 {
    void prep16c();                                                                   // 0x4a23a0
    int run16c(int a) { prep16c(); return handle(a); }
    int run16cb(int a) { prep16c(); return handle2(a); }
};
struct C4a1250 : P4a10, P4a158, P4a178, P4a190, P4a116c {
    int dispatch(int a);
    int dispatch2(int a);
};
// MATCH: golf_clean.exe 0x004a1250 ?dispatch@C4a1250@@QAEHH@Z
int C4a1250::dispatch(int a)
{
    int r;
    if ((r = run58(a)) != 0)
        return r;
    if ((r = run0(a)) != 0)
        return r;
    if ((r = run78(a)) != 0)
        return r;
    if ((r = handle90(a)) != 0)
        return r;
    return run16c(a);
}
// MATCH: golf_clean.exe 0x004a12e0 ?dispatch2@C4a1250@@QAEHH@Z
int C4a1250::dispatch2(int a)
{
    int r;
    if ((r = run58b(a)) != 0)
        return r;
    if ((r = run0b(a)) != 0)
        return r;
    if ((r = run78b(a)) != 0)
        return r;
    if ((r = handle90b(a)) != 0)
        return r;
    return run16cb(a);
}
struct N49e { int m_0; int m_data; int m_8; N49e* m_next; N49e* m_prev; };
struct L49e {
    int m_0, m_4;
    N49e* m_head;
    N49e* m_cur;
    int m_count;
    int m_index;
    void find(int data)
    {
        if (m_head) {
            m_index = 0;
            m_cur = m_head;
            for (int i = 0; i < m_count; i++) {
                if (m_cur->m_data == data)
                    return;
                m_index++;
                m_cur = m_cur->m_next;
            }
        }
    }
};
struct V49e { char pad[0xc0]; L49e m_list; char pad2[0xf0 - 0xd8]; unsigned m_f0; };
struct V049e { int m_0; };
struct C49eef0 : virtual V049e, virtual V49e { void setFlag(int id, int on); };
// MATCH: golf_clean.exe 0x0049eef0 ?setFlag@C49eef0@@QAEXHH@Z
void C49eef0::setFlag(int id, int on)
{
    m_list.find(id);
    if (on)
        m_f0 |= 1 << m_list.m_index;
    else
        m_f0 &= ~(1 << m_list.m_index);
}
struct W480 { VP16(a) VP16(b) VP16(c) VP4(d) virtual void getRect(RECT*); };
struct Mgr480 { VP4(a) VP4(b) VP4(c) virtual void invalidate(RECT*); };
extern Mgr480* g_83ad50d;
struct C480ce0 {
    int check();                      // 0x4801f0
    void getOrigin(int*, int*);       // 0x47b170
    void refresh();
    char pad[0x278];
    W480* m_278;
};
// MATCH: golf_clean.exe 0x00480ce0 ?refresh@C480ce0@@QAEXXZ
void C480ce0::refresh()
{
    if (!check())
        return;
    RECT r;
    if (m_278)
        m_278->getRect(&r);
    int x = 0, y = 0;
    getOrigin(&x, &y);
    r.left += x;
    r.right += x;
    r.top += y;
    r.bottom += y;
    g_83ad50d->invalidate(&r);
}
struct V0490 { int m_0; };
struct V490 { char pad[0xd0]; int m_count; char pad2[0xf0 - 0xd4]; int m_f0; int getSel(); };   // 0x489950
struct P0490 { char pad[0x5c0]; int m_5c0; char pad2[0x1464 - 0x5c4]; };
struct P1464 : virtual V0490, virtual V490 {
    int count1464() { return m_count; }
    int sel1464() { return getSel(); }
    char pad[0x70];
};
struct P14d8 : virtual V0490, virtual V490 {
    int flags14d8() { return m_f0; }
    char pad[0x1654 - 0x14dc];
};
struct C490a40 : P0490, P1464, P14d8 {
    int update(int);
    int m_1654;
    char pad3[0x1f68 - 0x1658];
    int m_1f68;
};
// MATCH: golf_clean.exe 0x00490a40 ?update@C490a40@@QAEHH@Z
int C490a40::update(int)
{
    if (m_1f68 & 0x4000)
        return 0;
    m_5c0 = 0;
    if (m_1654 != -1 && P1464::m_count) {
        if (m_1654 == 1) {
            m_5c0 = flags14d8();
            return 1;
        }
        m_5c0 = P1464::getSel();
    }
    return 1;
}
struct V049f {
    virtual void onMouse(int x, int y);
    VP16(a) VP16(b) VP16(c) VP16(d) VP4(e) virtual void v69(); virtual void v70(); virtual void v71();
    virtual void refresh();
};
struct V49f { char pad[0x48]; int m_48; char pad2[0x5c - 0x4c]; int m_5c; char pad3[0x68 - 0x60]; int m_68, m_6c; char pad4[0xf0 - 0x70]; unsigned m_f0; int m_f4; };
struct C49fda0 : virtual V049f, virtual V49f {
    virtual void onMouse(int x, int y);
    char pad[0x18];
};
// MATCH: golf_clean.exe 0x0049fda0 ?onMouse@C49fda0@@UAEXHH@Z
void C49fda0::onMouse(int x, int y)
{
    int cell = x / (m_68 + m_48) * m_5c + y / (m_6c + m_48);
    if (m_f0 & (1 << cell)) {
        if (m_f4 == -1)
            return;
        m_f4 = -1;
    } else {
        m_f4 = cell;
    }
    refresh();
}
struct W486 { VP16(a) VP16(b) VP16(c) VP4(d) virtual void v52(); virtual void v53(); virtual int width(); };
struct T486 { void wrap(char* s, int w, char** lines, int* count); };   // 0x476e20
struct C4862b0 {
    void layout(char** lines, int* count);
    char pad[0x274];
    T486 m_274;
    char pad1[3];
    W486* m_278;
    char pad2[0x574 - 0x27c];
    char* m_574;
    char pad3[0x598 - 0x578];
    int m_598, m_59c;
};
// MATCH: golf_clean.exe 0x004862b0 ?layout@C4862b0@@QAEXPAPADPAH@Z
void C4862b0::layout(char** lines, int* count)
{
    *count = 1;
    lines[0] = m_574;
    int margin = m_59c + m_598;
    m_274.wrap(m_574, (m_278 ? m_278->width() : 0) - margin * 2, lines, count);
    lines[*count] = m_574 + strlen(m_574);
}
extern char g_839658[];
extern int g_839a9c, g_839aa0, g_839aa4;
struct C478970 {
    void drawText(const char* s, int x, int y, int len);     // 0x477c30
    int textWidth();                                         // 0x477580
    void put(const char* s)
    {
        if (s)
            drawText(s, g_839a9c, g_839aa0, strlen(s));
    }
    int flush();
    char pad[0x1c];
    int m_1c;
    char pad2[0x30 - 0x20];
    int m_30, m_34, m_38, m_3c;
};
// MATCH: golf_clean.exe 0x00478970 ?flush@C478970@@QAEHXZ
int C478970::flush()
{
    if (g_839658[0]) {
        put(g_839658);
        g_839aa0 += textWidth();
        m_1c++;
    }
    g_839658[0] = 0;
    g_839aa4 = 0;
    m_30 = 0;
    m_38 = 0;
    m_34 = 0;
    m_3c = 0;
    return g_839aa0;
}
struct R46d { char m_0; char m_1; char pad[0x80]; short m_82; char pad2[0x9e - 0x84]; short m_9e; char pad3[0x100 - 0xa0]; };
extern R46d g_5794d8[152];
struct T46d { char m_0; char pad[0x207]; };
extern T46d g_575ab0[19];
extern int g_56a51c, g_59ae7c;
int alloc421bc0(int);
// MATCH: golf_clean.exe 0x0046d040 ?init46d040@@YAXXZ
void init46d040()
{
    int i;
    for (i = 0; i < 152; i++)
        g_5794d8[i].m_1 = 0;
    g_56a51c = 0;
    g_59ae7c = 0;
    for (i = 18; i >= 0; i--) {
        if (g_575ab0[i].m_0) {
            for (int j = 0; j < 2; j++) {
                int k = alloc421bc0(1);
                g_5794d8[k].m_1 = i;
                g_5794d8[k].m_9e = g_56a51c++;
                g_5794d8[k].m_82 = k ^ 1;
            }
        }
    }
}
int dist467170(int, int);
struct Pt438 { short x, y; };
extern Pt438 g_4c7b38[];
// MATCH: golf_clean.exe 0x00438260 ?nearest438260@@YAHHH@Z
int nearest438260(int x, int y)
{
    int i = 0;
    int best = 40;
    int idx = -1;
    do {
        int d;
        if (i < 8)
            d = dist467170((x - g_4c7b38[i].x) / 3, y - g_4c7b38[i].y);
        else
            d = dist467170(x - g_4c7b38[i].x, y - g_4c7b38[i].y);
        if (d < best) {
            best = d;
            idx = i;
        }
        i++;
    } while (g_4c7b38[i].x);
    return idx;
}
struct Pt4382 { int x, y; };
extern Pt4382 g_4c7be0[10];
// MATCH: golf_clean.exe 0x004382f0 ?zone4382f0@@YAHHH@Z
int zone4382f0(int x, int y)
{
    for (int i = 0; i < 10; i++) {
        if (dist467170(x - g_4c7be0[i].x - 60, y - g_4c7be0[i].y - 60) < 60)
            return i;
    }
    if (dist467170(x - 0x30a, y - 0x16b) < 60)
        return 10;
    return dist467170(x - 18, y - 0x1ef) < 60 ? 11 : -1;
}
int create4840e0(void*, int, int);
struct I484e { VP16(a) virtual void v16(); virtual void v17(); virtual void setOn(int); VP4(b) VP4(c) virtual void setFlags(int);
    VP4(d) virtual void v32(); virtual int run(); };
struct C484e70 {
    VP16(a) VP4(b) virtual void v20(); virtual void v21(); virtual int isX(); VP4(c) VP4(d) virtual void onStart();
    int start();
    char pad[0x30 - 4];
    int m_30;
    char pad2[0x40 - 0x34];
    I484e* m_40;
    unsigned m_44;
    char pad3[0x50 - 0x48];
    int m_50;
    char pad4[0x58 - 0x54];
    unsigned char m_58;
};
// MATCH: golf_clean.exe 0x00484e70 ?start@C484e70@@QAEHXZ
int C484e70::start()
{
    int flags = 0;
    int r;
    if (m_50) {
        if (!m_40) {
            int e = create4840e0(&m_40, m_50, 1);
            if (e)
                return e;
        }
        if (m_58 & 1)
            flags = 1;
        if (isX())
            flags |= 2;
        m_40->setFlags(flags);
        r = m_40->run();
        if (!r && !(m_44 & 1)) {
            m_44 |= 1;
            onStart();
            if (m_30)
                m_40->setOn(1);
        }
    } else {
        r = 8;
    }
    return r;
}
int check4378a0(int);
void f483bd0();
struct S5aa6d0 { void setOwner(int); void play(int, int, int, int); void finish(); };   // 0x487050, 0x487090, 0x487060
extern S5aa6d0 g_5aa6d0;
struct S519a60 { void post(int); };            // 0x4808c0
extern S519a60 g_519a60;
struct S83ad80 { int get(); };                 // 0x487630
extern S83ad80 g_83ad80h;
struct Mgr43c { VP4(a) VP4(b) virtual int window(); };
extern Mgr43c* g_83ad50h;
extern int g_5aa774;
// MATCH: golf_clean.exe 0x0043cce0 ?play43cce0@@YAXHH@Z
void play43cce0(int a, int b)
{
    if (!check4378a0(a))
        return;
    if (!b)
        g_5aa774 = timeGetTime() + 7000;
    else
        g_5aa774 = 0;
    g_5aa6d0.setOwner(g_83ad50h->window());
    f483bd0();
    g_519a60.post(0x80007fff);
    g_5aa6d0.play(a, 0, 0, g_83ad80h.get());
    g_5aa6d0.finish();
    g_5aa774 = 0;
}
struct Fmt479 { int m_bpp; };
struct Surf479 { VP4(a) VP4(b) virtual void v8(); virtual void unlock(int); VP16(c) VP16(d) VP4(e) VP4(f) VP4(g) virtual void v54(); virtual void v55(); virtual void v56(); virtual Fmt479* format(); };
struct Mgr479 { VP16(a) VP16(b) VP4(c) VP4(d) VP4(e) virtual void v44(); virtual int is565(); };
extern Mgr479* g_83ad50i;
struct C479830 {
    unsigned short* lock(int x, int y);      // 0x4796a0
    Fmt479* format() { return m_4 ? m_4->format() : 0; }
    unsigned char red(int x, int y, unsigned char def);
    unsigned char green(int x, int y, unsigned char def);
    int m_0;
    Surf479* m_4;
};
// MATCH: golf_clean.exe 0x00479830 ?red@C479830@@QAEEHHE@Z
unsigned char C479830::red(int x, int y, unsigned char def)
{
    unsigned short* p = lock(x, y);
    if (!p)
        return def;
    switch (format()->m_bpp) {
    case 24:
    case 32:
        def = ((unsigned char*)p)[2];
        break;
    case 16:
        {
            int v = *p;
            if (g_83ad50i->is565() == 1)
                def = (v >> 8) & 0xf8;
            else
                def = (v >> 7) & 0xf8;
        }
        break;
    }
    if (m_4)
        m_4->unlock(1);
    return def;
}
// MATCH: golf_clean.exe 0x004798c0 ?green@C479830@@QAEEHHE@Z
unsigned char C479830::green(int x, int y, unsigned char def)
{
    unsigned short* p = lock(x, y);
    if (!p)
        return def;
    switch (format()->m_bpp) {
    case 24:
    case 32:
        def = ((unsigned char*)p)[1];
        break;
    case 16:
        {
            int v = *p;
            if (g_83ad50i->is565() == 1)
                def = (v >> 3) & 0xfc;
            else
                def = (v >> 2) & 0xf8;
        }
        break;
    }
    if (m_4)
        m_4->unlock(1);
    return def;
}
struct Heap4a0 { char* alloc(int); };          // 0x474860
struct Out4a0 { int m_0; char* m_4; int m_8, m_c; };
struct C4a0540 {
    int copyTo(Out4a0* out);
    char pad[0x18];
    Heap4a0* m_18;
    char* m_1c;
    int m_20, m_24;
};
// MATCH: golf_clean.exe 0x004a0540 ?copyTo@C4a0540@@QAEHPAUOut4a0@@@Z
int C4a0540::copyTo(Out4a0* out)
{
    if (m_1c) {
        if (m_18)
            out->m_4 = m_18->alloc(strlen(m_1c) + 1);
        else
            out->m_4 = (char*)malloc(strlen(m_1c) + 1);
        if (!out->m_4)
            return 4;
        *out->m_4 = 0;
        strcat(out->m_4, m_1c);
    }
    out->m_8 = m_20;
    out->m_c = m_24;
    return 0;
}
struct C473b50 {
    void reset();          // 0x473ae0
    int open(const char* s, int w, int h);
    int m_0, m_4;
    char* m_8;
    int m_c, m_10, m_14, m_18, m_1c, m_20, m_24;
};
// MATCH: golf_clean.exe 0x00473b50 ?open@C473b50@@QAEHPBDHH@Z
int C473b50::open(const char* s, int w, int h)
{
    if (!s)
        return 16;
    reset();
    m_8 = (char*)malloc(strlen(s) + 1);
    if (!m_8)
        return 4;
    *m_8 = 0;
    strcat(m_8, s);
    m_20 = 0;
    m_24 = 0;
    m_10 = w;
    m_14 = h;
    m_18 = w;
    m_1c = h;
    return 0;
}
struct C48e010 { char pad[0x5c8]; char* m_5c8; int setName(const char* s); };
// MATCH: golf_clean.exe 0x0048e010 ?setName@C48e010@@QAEHPBD@Z
int C48e010::setName(const char* s)
{
    if (!s)
        return 3;
    if (m_5c8) {
        free(m_5c8);
        m_5c8 = 0;
    }
    m_5c8 = (char*)malloc(strlen(s) + 1);
    if (!m_5c8)
        return 4;
    *m_5c8 = 0;
    strcat(m_5c8, s);
    return 0;
}
struct V048c {
    virtual void scroll(int delta, int, int);
    VP16(a) VP16(b) VP16(c) VP16(d) VP4(e) virtual void v69(); virtual void v70(); virtual void v71();
    virtual void refresh();
    void setPos(int);      // 0x47b9f0
    char pad[0x98];
    unsigned m_9c;
};
struct V48c { char pad[0x5c]; int m_5c; char pad2[0xd0 - 0x60]; int m_count; };
struct C48caf0 : virtual V048c, virtual V48c {
    virtual void scroll(int delta, int, int);
    char pad[0x20];
    int m_24;
    char pad2[0x58 - 0x28];
};
// MATCH: golf_clean.exe 0x0048caf0 ?scroll@C48caf0@@UAEXHHH@Z
void C48caf0::scroll(int delta, int, int)
{
    if (m_9c & 8) {
        if (delta > 0) {
            if (m_24 > 0) {
                m_24--;
                setPos(m_24);
                refresh();
            }
        } else {
            if (m_24 < m_count - m_5c) {
                m_24++;
                setPos(m_24);
                refresh();
            }
        }
    }
}
struct V401 { virtual ~V401(); };
struct Obj401 : virtual V401 { };
struct Node401 : virtual V401 { int m_4; Obj401* m_obj; Node401* m_next; };
struct C401c70 : virtual V401 {
    virtual void v0();
    virtual void remove(Obj401*);
    ~C401c70();
    void clear()
    {
        if (m_head) {
            for (int i = 0; i < m_count; i++) {
                m_next = m_head->m_next;
                Obj401* o = m_head->m_obj;
                remove(o);
                if (o)
                    delete o;
                m_head->m_obj = 0;
                if (m_head)
                    delete m_head;
                m_head = m_next;
            }
            m_14 = 0;
            m_head = 0;
            m_count = 0;
        }
        m_14 = 0;
    }
    Node401* m_head;
    Node401* m_next;
    int m_count;
    int m_14;
    int m_18;
};
// MATCH: golf_clean.exe 0x00401c70 ??1C401c70@@UAE@XZ
C401c70::~C401c70()
{
    clear();
}
struct V4021 { virtual ~V4021(); };
struct Obj4021 : virtual V4021 { };
struct Node4021 : virtual V4021 { int m_4; Obj4021* m_obj; Node4021* m_next; };
struct C4021e0 : virtual V4021 {
    virtual void v0();
    virtual void remove(Obj4021*);
    ~C4021e0();
    void clear()
    {
        if (m_head) {
            for (int i = 0; i < m_count; i++) {
                m_next = m_head->m_next;
                Obj4021* o = m_head->m_obj;
                remove(o);
                if (o)
                    delete o;
                m_head->m_obj = 0;
                if (m_head)
                    delete m_head;
                m_head = m_next;
            }
            m_14 = 0;
            m_head = 0;
            m_count = 0;
        }
        m_14 = 0;
    }
    Node4021* m_head;
    Node4021* m_next;
    int m_count;
    int m_14;
    int m_18;
};
// MATCH: golf_clean.exe 0x004021e0 ??1C4021e0@@UAE@XZ
C4021e0::~C4021e0()
{
    clear();
}
struct I474d { virtual void v0(); virtual int create(int, int, int, int); };
struct Mgr474d { VP16(a) VP4(b) VP4(c) VP4(d) virtual void v28(); virtual void v29(); virtual void v30(); virtual I474d* make(void*, int); };
extern Mgr474d* g_83ad50k;
extern int g_83ad44;
struct C474dd0 {
    void release();                        // 0x474cb0
    void f478a20();
    void f478a70();
    void f4762d0(int, int, int, int);
    int create(int a, int b, int c, int mode, int tag, int);
    int m_0;
    I474d* m_4;
    int m_8;
    char pad[0x20 - 0xc];
    int m_20;
};
// MATCH: golf_clean.exe 0x00474dd0 ?create@C474dd0@@QAEHHHHHHH@Z
int C474dd0::create(int a, int b, int c, int mode, int tag, int)
{
    release();
    if (!mode) {
        if (!m_8)
            m_8 = 1;
    } else {
        m_8 = mode;
    }
    m_4 = g_83ad50k->make(this, m_8);
    int r = m_4->create(a, b, c, m_8);
    if (r) {
        release();
        return r;
    }
    f478a20();
    f478a70();
    f4762d0(g_83ad44, 0, 0, 0);
    m_20 = tag;
    return 0;
}
struct I475 { VP16(a) virtual int blit(void*, RECT*, RECT*); };
struct C475d00 {
    int blit(C475d00* src, int x, int y, int w, int h, int x2, int y2, int w2, int h2);
    int m_0;
    I475* m_4;
};
// MATCH: golf_clean.exe 0x00475d00 ?blit@C475d00@@QAEHPAU1@HHHHHHHH@Z
int C475d00::blit(C475d00* src, int x, int y, int w, int h, int x2, int y2, int w2, int h2)
{
    if (!src)
        return 16;
    if (m_4 && src->m_4) {
        RECT r1;
        RECT r2;
        r1.left = x;
        r1.top = y;
        r1.right = x + w;
        r1.bottom = y + h;
        r2.left = x2;
        r2.top = y2;
        r2.right = x2 + w2;
        r2.bottom = y2 + h2;
        return m_4->blit(src->m_4, &r1, &r2);
    }
    return 7;
}
struct V4a33 { virtual ~V4a33(); };
struct Obj4a33 : virtual V4a33 { };
struct Node4a33 : virtual V4a33 { int m_4; Obj4a33* m_obj; Node4a33* m_next; };
struct C4a33e0 : virtual V4a33 {
    virtual void v0();
    virtual void remove(Obj4a33*);
    ~C4a33e0();
    void clear()
    {
        if (m_head) {
            for (int i = 0; i < m_count; i++) {
                m_next = m_head->m_next;
                Obj4a33* o = m_head->m_obj;
                remove(o);
                if (o)
                    delete o;
                m_head->m_obj = 0;
                if (m_head)
                    delete m_head;
                m_head = m_next;
            }
            m_14 = 0;
            m_head = 0;
            m_count = 0;
        }
        m_14 = 0;
    }
    Node4a33* m_head;
    Node4a33* m_next;
    int m_count;
    int m_14;
    char pad[0x28 - 0x18];
};
// MATCH: golf_clean.exe 0x004a33e0 ??1C4a33e0@@UAE@XZ
C4a33e0::~C4a33e0()
{
    clear();
}
struct C4a29c0 { char pad[0x2c]; char* m_str[8]; int setStr(const char* s, int i); };
// MATCH: golf_clean.exe 0x004a29c0 ?setStr@C4a29c0@@QAEHPBDH@Z
int C4a29c0::setStr(const char* s, int i)
{
    if (!s) {
        if (m_str[i])
            *m_str[i] = 0;
        return 0;
    }
    if (m_str[i])
        free(m_str[i]);
    m_str[i] = (char*)malloc(strlen(s) + 1);
    if (!m_str[i])
        return 4;
    *m_str[i] = 0;
    strcat(m_str[i], s);
    return 0;
}
extern char g_5722e8[][50];
extern short g_53caf0[][50];
struct T407 { char m_0, m_1; signed char m_2; char pad[0x2d]; };
extern T407 g_578370[];
extern int g_4c2878[8];
extern int g_4c2898[8];
int blocked40bf60(int, int);
// MATCH: golf_clean.exe 0x00407400 ?canStep407400@@YAHHHH@Z
int canStep407400(int x, int y, int dir)
{
    char t = g_5722e8[x][y];
    if (g_578370[t].m_2 > 0 && t != 0x16 && t != 0x15) {
        int nx = g_4c2878[dir] + x;
        int ny = g_4c2898[dir] + y;
        if (!(g_53caf0[nx][ny] & 0x120)) {
            if (g_578370[g_5722e8[nx][ny]].m_2 > 0) {
                if (!blocked40bf60(nx, ny))
                    return 1;
            }
        }
    }
    return 0;
}
struct V04a4 {
    virtual void onPick(int a, int b);
    VP16(a) VP16(b) VP16(c) VP16(d) VP4(e) virtual void v69(); virtual void v70(); virtual void v71(); virtual void v72();
    virtual void redrawAll();
};
struct V4a4 { char pad[0xf0]; int m_f0; };
struct F4a4 { int find(int, int, int*, int); };       // 0x492a90
struct C4a4350 : virtual V04a4, virtual V4a4 {
    virtual void onPick(int a, int b);
    int toggle(int);        // 0x4a4890
    void redraw(int);       // 0x4a3f10
    char pad[0xc];
    F4a4 m_10;
    char pad2[0xbc - 0x11];
    void (*m_bc)(int);
    char pad3[0xe0 - 0xc0];
};
// MATCH: golf_clean.exe 0x004a4350 ?onPick@C4a4350@@UAEXHH@Z
void C4a4350::onPick(int a, int b)
{
    if (m_10.find(a, b, &b, 0) == -1)
        return;
    if (m_bc)
        m_bc(b);
    int old = m_f0;
    m_f0 = toggle(b);
    redraw(old);
    redraw(m_f0);
    redrawAll();
}
