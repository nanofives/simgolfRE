// FLAGS golf_clean.exe: /O2 /GX
// probe
#include <windows.h>
struct A479 { A479(); ~A479(); virtual void a(); char pad[0x270]; };   // ctor 0x479c40
struct B474 { B474(); virtual void b(); };                     // ctor 0x474ae0
struct C4804a0 : A479, B474 {
    C4804a0();
    ~C4804a0();
    virtual void a();
    virtual void b();
    char pad2[0x574 - 0x278];
};
struct V489150 { V489150(); virtual void c(); char pad[0x10]; };
extern int g_4e4a24, g_4e4a28;
struct C49ebb0 : virtual C4804a0, virtual V489150 {
    C49ebb0();
    virtual void a();
    virtual void b();
    virtual void c();
    int m_4, m_8, m_c;
    int m_10, m_14;
};
// MATCH: golf_clean.exe 0x0049ebb0 ??0C49ebb0@@QAE@XZ
C49ebb0::C49ebb0()
{
    m_4 = 0;
    m_8 = 0;
    m_c = 0;
    m_14 = g_4e4a28;
    m_10 = g_4e4a24;
}
struct Surf474 { Surf474(); ~Surf474(); int load(const char* name, void* pal, int a, int b, int c); char pad[0x2b8]; };   // 0x474ae0, 0x474c40, 0x475840
struct Img840710 { int init(int w, int h); int make(Surf474* s, int a, int b, int w, int h, int c, int d); };    // 0x4745c0, 0x473bf0
extern Img840710 g_840710;
extern const char s_004e49fc[];
// MATCH: golf_clean.exe 0x0049d1b0 ?load49d1b0@@YAHXZ
int load49d1b0()
{
    Surf474 s;
    int r;
    if (s.load(s_004e49fc, 0, 10, 0xec, 0))
        r = g_840710.init(16, 16);
    else
        r = g_840710.make(&s, 1, 0x12, 16, 16, 1, 1);
    if (r)
        return r;
    return 0;
}
struct Terrain {
    void passCollarInfo(int* info, int n);
    void initSystem(int w, int h, HDC dc, bool nt5);
    void initTerrain();
};
struct Win519 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual HDC getDC(); };
extern Win519* g_519cd8;
extern Terrain* g_820ed0;
extern bool g_820f28;
extern int g_820f30, g_820f34;
extern char g_5a34e0;
struct Kind578 { char m_0; char pad[3]; char m_4; char pad2[0x30 - 5]; };
extern Kind578 g_578372[23];
void finish449540();
// MATCH: golf_clean.exe 0x00449790 ?initTerrain449790@@YAXHH@Z
void initTerrain449790(int w, int h)
{
    int info[23];
    OSVERSIONINFOA vi;
    vi.dwOSVersionInfoSize = sizeof(vi);
    GetVersionExA(&vi);
    if (vi.dwMajorVersion >= 5)
        g_820f28 = true;
    g_820f30 = w;
    g_820f34 = h;
    for (int i = 0; i < 23; i++) {
        info[i] = g_578372[i].m_4;
        if (g_5a34e0 == 1 && g_578372[i].m_0 > 0 && i != 5 && i != 7)
            info[i] = 4;
    }
    g_820ed0->passCollarInfo(info, 23);
    g_820ed0->initSystem(w, h, g_519cd8->getDC(), g_820f28);
    g_820ed0->initTerrain();
    finish449540();
}
