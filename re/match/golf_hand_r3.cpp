// FLAGS golf_clean.exe: /O2 /GX
// golf_clean.exe batch r3 written by hand (release /O2, /GX for the EH frames). Names are chosen here, not recovered.
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#pragma warning(disable: 4715)
#pragma vtordisp(off)

// Base 0x485260 (dtor 0x4852d0, the EH unwind action of state 0). The m_44 clear is a 4-byte memset followed by a
// bit-0 clear (`mov ecx, edi; and ecx, ~1` survives only in this shape).
struct r3_B485260 { r3_B485260(); ~r3_B485260(); virtual void v(); char pad[0x3c]; };
struct r3_F44 { unsigned b0 : 1; unsigned b1 : 1; unsigned b2 : 1; };
struct r3_C484150 : r3_B485260 {
    r3_C484150();
    virtual void v();
    void setRate(int);   // 0x4846d0
    __declspec(nothrow) void setMode(int);   // 0x484260; nothrow: 0x484820 has no EH frame although its base has a dtor
    int m_40;
    r3_F44 m_44;
    int m_48, m_4c, m_50, m_54;
};
// MATCH: golf_clean.exe 0x00484150 ??0r3_C484150@@QAE@XZ
r3_C484150::r3_C484150()
{
    m_48 = 0;
    m_4c = 0;
    m_40 = 0;
    m_50 = 0;
    memset(&m_44, 0, 4);
    m_44.b0 = 0;
    setRate(1000);
    m_54 = 0;
}
// m_58 is cleared by a 4-byte memset (lea eax,[esi+0x58]; mov [eax],0), which also keeps the later zeros immediate.
struct r3_C484820 : r3_C484150 {
    r3_C484820();
    virtual void v();
    int m_58, m_5c;
    float m_60;
    int m_64, m_68;
};
// MATCH: golf_clean.exe 0x00484820 ??0r3_C484820@@QAE@XZ
r3_C484820::r3_C484820()
{
    memset(&m_58, 0, 4);
    m_44.b2 = 1;
    setMode(1);
    m_5c = 0;
    m_60 = 1.0f;
    *(int*)&pad[0x38] = 0x10;
    m_64 = 0;
    m_68 = 0;
}

// Base 0x487210 (dtor 0x487260); 16 elements of 0x1c bytes at +0x24 (ctor 0x4877d0, dtor 0x487310).
struct r3_B487210 { r3_B487210(); ~r3_B487210(); virtual void v(); char pad[0x1c]; };
struct r3_E4877d0 { r3_E4877d0(); ~r3_E4877d0(); char pad[0x1c]; };
struct r3_C487280 : r3_B487210 { r3_C487280(); virtual void v(); int m_20; r3_E4877d0 m_24[16]; };
// MATCH: golf_clean.exe 0x00487280 ??0r3_C487280@@QAE@XZ
r3_C487280::r3_C487280()
{
    memset(&m_20, 0, 4);
}

// Base dtor 0x4841e0; m_40 has a virtual scalar-deleting dtor at slot 0; 0x4a4ffc is operator delete.
struct r3_P485 { virtual ~r3_P485(); };
struct r3_B4841e0 { ~r3_B4841e0(); virtual void v(); char pad[0x3c]; };
struct r3_C4853d0 : r3_B4841e0 {
    ~r3_C4853d0();
    virtual void v();
    r3_P485* m_40;
    int m_44[3];
    void* m_50;
};
// MATCH: golf_clean.exe 0x004853d0 ??1r3_C4853d0@@QAE@XZ
r3_C4853d0::~r3_C4853d0()
{
    if (m_40) {
        delete m_40;
        m_40 = 0;
    }
    if (m_50) {
        operator delete(m_50);
        m_50 = 0;
    }
}

// 0x80-byte header copied to the stack (rep movsd), byte 3 must be 8. 0x495420/0x495100 take the same 5 args.
extern const char s_004e4218[];
int f492460();   // 0x492460
struct r3_Hdr478 { unsigned char b[0x80]; };
struct r3_C478cd0 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual int v4();
    int load(char* data, int size, int a, int b, int c);
    int loadA(char* data, int size, int a, int b, int c);   // 0x495420
    int loadB(char* data, int size, int a, int b, int c);   // 0x495100
};
// MATCH: golf_clean.exe 0x00478cd0 ?load@r3_C478cd0@@QAEHPADHHHH@Z
int r3_C478cd0::load(char* data, int size, int a, int b, int c)
{
    r3_Hdr478 h;
    if (!data)
        return 3;
    if (!strncmp(data, s_004e4218, strlen(s_004e4218))) {
        data += strlen(s_004e4218) + 1;
        size += -1 - strlen(s_004e4218);
    }
    h = *(r3_Hdr478*)data;
    if (h.b[3] != 8)
        return 0x17;
    if (v4() && f492460() != 8)
        return loadA(data, size, a, b, c);
    return loadB(data, size, a, b, c);
}

// Twelve 0x2c-byte members (vtable 0x4ba2d8) whose inline dtor calls 0x473ae0.
struct r3_T44b790 { ~r3_T44b790() { release(); } virtual void v0(); void release(); char pad[0x28]; };
struct r3_C44b790 {
    r3_T44b790 m_0, m_2c, m_58, m_84, m_b0, m_dc, m_108, m_134, m_160, m_18c, m_1b8, m_1e4;
    ~r3_C44b790();
};
// MATCH: golf_clean.exe 0x0044b790 ??1r3_C44b790@@QAE@XZ
r3_C44b790::~r3_C44b790()
{
}

// Appends the course-upgrade message to the text buffer 0x51a068; g_4c2a18 is a table of course-type names (i, i+1).
extern char g_51a068[];
extern char* g_4c2a18[];
extern const char s_004c54cc[];
extern const char s_004c54c4[];
extern const char s_004c5450[];
extern const char s_004c53ec[];
extern const char s_004c53a4[];
// MATCH: golf_clean.exe 0x0040e5f0 ?r3_upgradeText@@YAXH@Z
void r3_upgradeText(int i)
{
    strcat(g_51a068, s_004c54cc);
    strcat(g_51a068, g_4c2a18[i]);
    strcat(g_51a068, s_004c54c4);
    strcat(g_51a068, g_4c2a18[i + 1]);
    strcat(g_51a068, s_004c5450);
    if (i == 0)
        strcat(g_51a068, s_004c53ec);
    else if (i == 1)
        strcat(g_51a068, s_004c53a4);
}

// Height at a world point (1024 units per tile): tile-type flags at 0x57837c (stride 0x30), bilinear blend of the four corner values from 0x40bfe0.
struct r3_TileType { unsigned flags; char pad[0x2c]; };
extern r3_TileType g_57837c[];
extern char g_5722e8[][0x32];
void __cdecl r3_f42f4b0(int x, int y, int* a, int* b);   // 0x42f4b0
int __cdecl r3_f40bfe0(int x, int y, int d, int f);       // 0x40bfe0
// MATCH: golf_clean.exe 0x0042fa30 ?r3_heightAt@@YAHHH@Z
int r3_heightAt(int x, int y)
{
    int tx = x >> 10;
    int ty = y >> 10;
    int a, b;
    int h5, h7, h1, h3, fx, fy;
    unsigned f = g_57837c[g_5722e8[tx][ty]].flags;
    if (f & 8)
        return 0;
    if (f & 2) {
        r3_f42f4b0(tx, ty, &a, &b);
        return (b - 3) * 16;
    }
    if (f & 4) {
        r3_f42f4b0(tx, ty, &a, &b);
        return (a - 3) * 16;
    }
    h5 = r3_f40bfe0(tx, ty, 5, 1) - 3;
    h7 = r3_f40bfe0(tx, ty, 7, 1) - 3;
    h1 = r3_f40bfe0(tx, ty, 1, 1) - 3;
    h3 = r3_f40bfe0(tx, ty, 3, 1) - 3;
    if (h5 == h7 && h5 == h1 && h5 == h3)
        return h5 * 16;
    fx = x - (tx << 10);
    fy = y - (ty << 10);
    return (((0x400 - fx) * h7 + fx * h1) * (0x400 - fy) + ((0x400 - fx) * h5 + fx * h3) * fy) * 16 / 1024 / 1024;
}

// Field reset from defaults (0x4e4764.. and 0x83ff1c..); the members at 0x60c and 0xcdc are called through their vtables (slot 0x16c).
#define VP10(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3(); virtual void n##4(); virtual void n##5(); virtual void n##6(); virtual void n##7(); virtual void n##8(); virtual void n##9();
struct r3_V4961d0 { VP10(a) VP10(b) VP10(c) VP10(d) VP10(e) VP10(f) VP10(g) VP10(h) VP10(i) virtual void v90(); virtual void reset(); char pad[0x6d0 - 4]; };
extern int g_83ff1c, g_83ff20, g_83ff58, g_83ff5c;
extern int g_83ff24[3], g_83ff30[3], g_83ff3c[3], g_83ff48[3];
extern int g_4e4764, g_4e4768, g_4e476c, g_4e4770, g_4e4774, g_4e4778, g_4e477c, g_4e4780, g_4e4784, g_4e4788, g_4e478c;
struct r3_C4961d0 {
    char pad0[0x574];
    int m_574;
    int m_578;
    int m_57c;
    int m_580;
    int m_584;
    int m_588;
    int m_58c;
    int m_590;
    int m_594;
    int m_598;
    int m_59c;
    int m_5a0;
    int m_5a4;
    int m_5a8;
    int m_5ac;
    int m_5b0;
    int m_5b4;
    int m_5b8;
    int m_5bc;
    int m_5c0;
    int m_5c4;
    int m_5c8;
    int m_5cc;
    int m_5d0;
    int m_5d4;
    int m_5d8;
    int m_5dc[4][3];
    r3_V4961d0 m_60c;
    r3_V4961d0 m_cdc;
    int m_13ac, m_13b0;
    void f480610();   // 0x480610
    void init();
};
// MATCH: golf_clean.exe 0x004961d0 ?init@r3_C4961d0@@QAEXXZ
void r3_C4961d0::init()
{
    int i;
    m_574 = g_83ff1c;
    m_57c = g_4e4770;
    m_588 = 0;
    m_580 = g_83ff20;
    m_584 = g_4e4774;
    m_58c = g_83ff20;
    m_59c = -1;
    m_598 = 0;
    m_590 = g_4e476c;
    m_594 = g_4e4768;
    m_5a0 = g_4e4764;
    m_5a4 = 0;
    m_5ac = 0;
    m_5b0 = 0;
    m_5b4 = 0;
    m_5b8 = 0;
    m_5a8 = g_4e4778;
    m_5bc = g_4e477c;
    m_5c4 = g_4e4780;
    m_5c8 = g_4e4784;
    m_5cc = g_4e4788;
    m_5d0 = g_4e478c;
    for (i = 0; i < 3; i++) {
        m_5dc[0][i] = g_83ff24[i];
        m_5dc[1][i] = g_83ff30[i];
        m_5dc[2][i] = g_83ff3c[i];
        m_5dc[3][i] = g_83ff48[i];
    }
    m_5d4 = g_83ff58;
    m_5d8 = g_83ff5c;
    m_13ac = 0;
    m_13b0 = 0;
    (&m_60c)->reset();
    (&m_cdc)->reset();
    f480610();
}

// Flips a DIB vertically in place (8 or 24 bpp), swapping row hi with row lo through a one-row buffer.
// MATCH: golf_clean.exe 0x00475410 ?r3_flipRows@@YGXPAEPAUtagBITMAPINFOHEADER@@@Z
void __stdcall r3_flipRows(unsigned char* bits, BITMAPINFOHEADER* bi)
{
    int bpp = 1;
    unsigned char* tmp;
    int hi, lo, a, b, x;
    if (bi->biBitCount == 24)
        bpp = 3;
    tmp = new unsigned char[bpp * bi->biWidth];
    if (bi->biHeight % 2 == 0) {
        hi = abs(bi->biHeight) / 2;
        lo = hi - 1;
    } else {
        hi = abs(bi->biHeight) / 2 + 1;
        lo = hi - 2;
    }
    for (; hi < abs(bi->biHeight); hi++, lo--) {
        b = bi->biWidth * hi * bpp;
        a = bi->biWidth * lo * bpp;
        for (x = 0; x < bi->biWidth * bpp; x += bpp) {
            tmp[x] = bits[b + x];
            bits[b + x] = bits[a + x];
            bits[a + x] = tmp[x];
            if (bpp == 3) {
                tmp[x + 1] = bits[b + x + 1];
                tmp[x + 2] = bits[b + x + 2];
                bits[b + x + 1] = bits[a + x + 1];
                bits[b + x + 2] = bits[a + x + 2];
                bits[a + x + 1] = tmp[x + 1];
                bits[a + x + 2] = tmp[x + 2];
            }
        }
    }
    delete tmp;
}

// Per-tile value: 3 off-map or when blocked; the byte table 0x5a4998 (stride 0x33) once g_834170 is set; else derived from 0x42dba0.
extern unsigned g_flags;
extern int g_834170;
extern char g_5722e8[50][50];
extern unsigned char g_5a4998[50][51];
struct r3_Course571 { char site; char pad1; char m_2; char pad3; char m_4; char pad[0x29]; };
extern r3_Course571 g_571ff4[];
extern int g_59bf90;
extern int g_4c2fa0;
extern int g_822c88;
int __cdecl r3_blocked40bf60(int x, int y);   // 0x40bf60
int __cdecl r3_f42dba0(int x, int y);         // 0x42dba0
int __cdecl r3_f467130(int a, int b, int c);  // 0x467130
// MATCH: golf_clean.exe 0x0040c170 ?r3_f40c170@@YAHHH@Z
int r3_f40c170(int x, int y)
{
    char t;
    int h, s;
    if (!(g_flags & 1) && x >= 0 && x < 50 && y >= 0 && y < 50) {
        if (g_834170) {
            if (g_5722e8[x][y] == 0x14 && r3_blocked40bf60(x - 1, y) && r3_blocked40bf60(x, y + 1))
                return 3;
            if (g_834170)
                return g_5a4998[x][y];
        }
        s = y + x * 50;
        t = ((char*)g_5722e8)[s];
        if (t != 0x11 && ((char*)g_5722e8)[s + 50] != 0x11 && t != 0x12 && t != 0x13) {
            if (g_571ff4[g_59bf90].m_2 == 2)
                h = r3_f42dba0(x << 7, y << 7);
            else
                h = r3_f42dba0(x << 6, y << 6);
            if (g_571ff4[g_59bf90].m_4 == 1 && y < 16)
                h += (16 - y) * g_4c2fa0 / -6;
            return r3_f467130(h / g_4c2fa0 + 1, (g_822c88 != 0) + 3, 15);
        }
    }
    return 3;
}

// Window show: sets the shown bit (m_a0 |= 1), optional resize/draw, activates the current child, re-posts its rect moved to the origin.
struct r3_Rect { int l, t, r, b; void offset(int dx, int dy) { l += dx; r += dx; t += dy; b += dy; } };
struct r3_N47b { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void notify(); };
// Global display object at 0x83ad50.
struct r3_Scr { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void invalidate(r3_Rect*);
    virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); VP10(c) virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void setDirty(int, int, int); };
extern r3_Scr* g_83ad50;
struct r3_Win47b;
struct r3_Kid47b { VP10(a) VP10(b) VP10(c) VP10(d) VP10(e) VP10(f) virtual void setActive(int); };
struct r3_Node47b { int m_0; r3_Kid47b* m_4; };
struct r3_Par47b { VP10(a) VP10(b) VP10(c) VP10(d) VP10(e) VP10(f) VP10(g) virtual void v70(); virtual void v71(); virtual void layout(); };
struct r3_Win47b {
    VP10(a) VP10(b) virtual void v20(); virtual void v21(); virtual void draw();
    char pad4[0xc - 4];
    r3_N47b* m_c;
    char pad10[0x9c - 0x10];
    unsigned m_9c;
    unsigned m_a0;
    char pada4[0x138 - 0xa4];
    int m_138;
    r3_Node47b* m_13c;
    int m_140;
    char pad144[0x15c - 0x144];
    r3_Par47b* m_15c;
    char pad160[0x1ac - 0x160];
    r3_Rect m_1ac;
    r3_Rect m_1bc;
    char pad1cc[0x220 - 0x1cc];
    unsigned m_220;
    void f47e120();                 // 0x47e120
    void resize(int, int);          // 0x47bc60
    void setRect(r3_Rect*);         // 0x47b0d0
    void setRect2(r3_Rect*);        // 0x47b120
    void clipTo(void*);             // 0x479a80
    void show(unsigned char flags);
};
void r3_f47e450();                  // 0x47e450
extern void* g_83ab40;
extern void* g_83ab44;
extern r3_Win47b* g_83aa98;
extern char g_83aa78[];
// MATCH: golf_clean.exe 0x0047b670 ?show@r3_Win47b@@QAEXE@Z
void r3_Win47b::show(unsigned char flags)
{
    r3_Rect rc;
    if ((m_a0 & 1) || !(m_a0 & 4))
        return;
    m_220 |= 2;
    if (!(m_9c & 2)) {
        g_83ab40 = 0;
        g_83ab44 = 0;
    }
    m_a0 |= 1;
    if (!(flags & 4))
        f47e120();
    r3_f47e450();
    if (!(flags & 2))
        resize(m_1ac.r - m_1ac.l, m_1ac.b - m_1ac.t);
    if (!(flags & 1))
        draw();
    if (m_15c)
        m_15c->layout();
    if (m_140 && m_138 && m_13c->m_4)
        m_13c->m_4->setActive(1);
    if (m_a0 & 2) {
        rc = m_1bc;
        rc.offset(-rc.l, -rc.t);
        setRect2(&rc);
    } else {
        rc = m_1ac;
        rc.offset(-rc.l, -rc.t);
        setRect(&rc);
    }
    if (g_83ad50)
        g_83ad50->invalidate(&rc);
    if (m_c)
        m_c->notify();
    if (g_83aa98 == this)
        clipTo(g_83aa78);
}

// Nine-slice frame from the 16x16 pieces g_5a53c8[k] / g_58b680[k] (stride 0x2c); w and h are rounded up to 16 with x/y re-centered.
struct r3_Img58b { char pad[0x2c]; };
struct r3_Spr5a5 { void draw(r3_Img58b* img, void* dst, int x, int y, int f); char pad[0x2c]; };   // 0x473f60
extern r3_Spr5a5 g_5a53c8[9];
extern r3_Img58b g_58b680[9];
extern void* g_4c1570;
void __cdecl r3_f40ca10(int x, int y, int w, int h, int f);   // 0x40ca10
// MATCH: golf_clean.exe 0x0040cef0 ?r3_frame@@YAXHHHHH@Z
void r3_frame(int x, int y, int w, int h, int fill)
{
    int r, i;
    if (g_83ad50)
        g_83ad50->setDirty(1, 1, 1);
    r = w & 15;
    if (r) {
        w += 15 - r;
        x -= (15 - r) / 2;
    }
    r = h & 15;
    if (r) {
        h += 15 - r;
        y -= (15 - r) / 2;
    }
    if (fill)
        r3_f40ca10(x, y, w, h, 1);
    g_5a53c8[0].draw(&g_58b680[0], g_4c1570, x, y, 0);
    for (i = 16; i < w - 16; i += 16) {
        g_5a53c8[1].draw(&g_58b680[1], g_4c1570, x + i, y, 0);
        g_5a53c8[7].draw(&g_58b680[7], g_4c1570, x + i, y + h - 16, 0);
    }
    for (i = 16; i < h - 16; i += 16) {
        g_5a53c8[3].draw(&g_58b680[3], g_4c1570, x, y + i, 0);
        g_5a53c8[5].draw(&g_58b680[5], g_4c1570, x + w - 16, y + i, 0);
    }
    g_5a53c8[2].draw(&g_58b680[2], g_4c1570, x + w - 16, y, 0);
    g_5a53c8[8].draw(&g_58b680[8], g_4c1570, x + w - 16, y + h - 16, 0);
    g_5a53c8[6].draw(&g_58b680[6], g_4c1570, x, y + h - 16, 0);
}

// Button hover: rect of m_278 moved to the origin, hover timer m_5b4 and highlight via m_130 slots 0xcc/0xd0. The original keeps 'test m_620' with both
// paths making the same call, so the source has an if/else on m_620 there whose two bodies compile identically; what they differ in is not determined.
struct r3_P488 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void off(int); virtual void on(int); char pad4[0x78 - 4]; r3_N47b* m_78; r3_N47b* m_7c; };
struct r3_R488 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void getRect(r3_Rect*); };
struct r3_Timer488 { void stop(); void start(void (*fn)(), int, void*, int, int); };   // 0x486ec0, 0x486d60
void r3_cb488fb0();                                         // 0x488fb0
int __cdecl r3_ptIn492610(int x, int y, r3_Rect* r);        // 0x492610
extern void* g_83ab40;
extern void* g_83ab44;
extern int g_83ab48;
struct r3_Btn488 {
    VP10(a) VP10(b) VP10(c) virtual void mouse(int, int);   // 0x78
    VP10(d) VP10(e) VP10(f) VP10(g) virtual void v71(); virtual void redraw();   // 0x120
    char pad4[0x130 - 4];
    r3_P488* m_130;
    char pad134[0x278 - 0x134];
    r3_R488* m_278;
    char pad27c[0x5ac - 0x27c];
    int m_5ac;
    int m_5b0;
    r3_Timer488 m_5b4;
    char pad5b5[0x5e8 - 0x5b5];
    int m_5e8;
    char pad5ec[0x614 - 0x5ec];
    int m_614;
    char pad618[0x620 - 0x618];
    int m_620;
    int f47abc0();   // 0x47abc0
    void hover(int x, int y);
};
// MATCH: golf_clean.exe 0x00488cf0 ?hover@r3_Btn488@@QAEXHH@Z
void r3_Btn488::hover(int x, int y)
{
    r3_Rect r;
    void* f;
    if (m_278)
        m_278->getRect(&r);
    r.offset(-r.l, -r.t);
    f = g_83ab40;
    if (!f)
        f = g_83ab44;
    if (f != this || !g_83ab48)
        return;
    if (!r3_ptIn492610(x, y, &r) && m_614 == 1) {
        m_5b4.stop();
        m_614 = 0;
        if (m_130->m_78)
            m_130->m_78->notify();
        if (m_620)
            m_130->off(m_5e8);
        else
            m_130->off(m_5e8);
        redraw();
        return;
    }
    if (r3_ptIn492610(x, y, &r)) {
        if (m_614) {
            if (f47abc0())
                mouse(x, y);
        } else {
            if (m_5ac != -1)
                m_5b4.start(r3_cb488fb0, 1, this, m_5ac, 5);
            m_614 = 1;
            if (m_130->m_7c)
                m_130->m_7c->notify();
            if (m_620)
                m_130->on(m_5e8);
            else
                m_130->on(m_5e8);
            redraw();
        }
    }
}

// Builds frame/chunk pointer tables by walking size-prefixed chunks whose word at +4 is 0xf1fa.
struct r3_Hdr481 { char pad[0x50]; char* m_50; };
struct r3_Flc481 {
    char pad[0x44];
    char* m_44;
    char** m_48;
    char** m_4c;
    char pad50[0x58 - 0x50];
    r3_Hdr481* m_58;
    int index(unsigned short rows, unsigned short cols);
};
// MATCH: golf_clean.exe 0x00481e40 ?index@r3_Flc481@@QAEHGG@Z
int r3_Flc481::index(unsigned short rows, unsigned short cols)
{
    int i, j, k;
    char* p;
    if (!m_44)
        return 7;
    if (m_48)
        operator delete(m_48);
    m_48 = (char**)operator new(rows * 4);
    m_4c = (char**)operator new(cols * rows * 4);
    memset(m_4c, 0, cols * rows);
    if (!m_48)
        return 4;
    k = 0;
    p = m_58->m_50 + (int)m_44;
    for (i = 0; i < rows; i++) {
        m_48[i] = p;
        for (j = 0; j < cols + 1; j++) {
            if (j == cols && i == rows - 1)
                break;
            do
                p += *(int*)p;
            while (*(unsigned short*)(p + 4) != 0xf1fa);
            if (j != cols)
                m_4c[k++] = p;
        }
    }
    return 0;
}
