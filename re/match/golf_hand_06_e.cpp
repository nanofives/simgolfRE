// FLAGS golf_clean.exe: /O2 /GX
// probe
struct A479 { A479(); ~A479(); virtual void a(); char pad[0x270]; };   // ctor 0x479c40
struct B474 { B474(); virtual void b(); };                     // ctor 0x474ae0
extern int g_83ac18;
struct C4804a0 : A479, B474 {
    C4804a0();
    virtual void a();
    virtual void b();
    char pad2[0x52c - 0x278];
    int m_52c[16];
    int m_56c;
    int m_570;
};
// MATCH: golf_clean.exe 0x004804a0 ??0C4804a0@@QAE@XZ
C4804a0::C4804a0()
{
    m_570 = 0;
    *(int*)(pad + 0x1a4 - 4) = 0;
    *(int*)(pad + 0x1a8 - 4) = 0;
    m_52c[0] = 0;
    m_52c[1] = 0;
    m_52c[2] = 0;
    m_52c[3] = 0;
    m_52c[4] = 0;
    m_52c[5] = 0;
    m_52c[6] = 0;
    m_52c[7] = 0;
    m_52c[8] = 0;
    m_52c[9] = 0;
    m_52c[10] = 0;
    m_52c[11] = 0;
    m_52c[12] = 0;
    m_52c[13] = 0;
    m_52c[14] = 0;
    m_52c[15] = 0;
    m_56c = g_83ac18;
}
struct P0_491 { virtual void a(); char pad[0x270]; };
struct P1_491 { virtual void b(); };
struct B488650 : P0_491, P1_491 { B488650(); ~B488650(); char padb[0x630 - 0x278]; };   // ctor 0x488500, dtor 0x488650
extern int g_840948[3];
extern int g_840954[9][3];
struct C4a13f0 : B488650 {
    C4a13f0();
    ~C4a13f0() { shutdown(); }
    virtual void a();
    void shutdown();     // 0x4a14c0
    int m_630, m_634;
    int m_638[3];
    int m_644[3];
    int m_650[3];
    int m_65c, m_660;
    int m_664[9][3];
};
// MATCH: golf_clean.exe 0x004a13f0 ??0C4a13f0@@QAE@XZ
C4a13f0::C4a13f0()
{
    m_630 = 1000;
    *(int*)(padb + 0x578 - 0x278) = 0;
    *(int*)(padb + 0x574 - 0x278) = 0;
    m_634 = -1;
    m_65c = 0;
    m_660 = 0;
    for (int i = 0; i < 3; i++) {
        m_644[i] = g_840948[i];
        m_638[i] = -1;
        m_664[0][i] = g_840954[0][i];
        m_664[1][i] = g_840954[1][i];
        m_664[2][i] = g_840954[2][i];
        m_664[3][i] = g_840954[3][i];
        m_664[4][i] = g_840954[4][i];
        m_664[5][i] = g_840954[5][i];
        m_664[6][i] = g_840954[6][i];
        m_664[7][i] = g_840954[7][i];
        m_664[8][i] = g_840954[8][i];
        m_650[i] = 0;
    }
}
struct B4805a0 : A479, B474 { ~B4805a0(); char padc[0x60c - 0x278]; };   // dtor 0x4805a0
struct C497a20 : B4805a0 {
    ~C497a20();
    virtual void a();
    void cleanup();      // 0x4961d0
    C4a13f0 m_60c;
    C4a13f0 m_cdc;
};
// MATCH: golf_clean.exe 0x00497a20 ??1C497a20@@QAE@XZ
C497a20::~C497a20()
{
    cleanup();
}
