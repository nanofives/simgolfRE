// FLAGS golf_clean.exe: /O2 /GX
// probe
struct A479 { A479(); ~A479(); virtual void a(); char pad[0x270]; };
struct B474 { B474(); virtual void b(); };
struct C4804a0 : A479, B474 {
    C4804a0();
    ~C4804a0();
    virtual void a();
    virtual void b();
    char pad2[0x574 - 0x278];
};
struct V489150 { V489150(); virtual void c(); char pad[0x10]; };
extern int g_8409c4;
struct C4a2250 : virtual C4804a0, virtual V489150 {
    C4a2250();
    virtual void a();
    virtual void b();
    virtual void c();
    int m_4[10];
    int m_2c[10];
    int m_54[10];
    int m_7c, m_80, m_84;
};
// MATCH: golf_clean.exe 0x004a2250 ??0C4a2250@@QAE@XZ
C4a2250::C4a2250()
{
    for (int i = 0; i < 10; i++) {
        m_4[i] = 0;
        m_2c[i] = 0;
        m_54[i] = 10;
    }
    m_84 = g_8409c4;
}
struct Sub4a08 { int handle(int a, int b, int c, int d); };
struct C4a08a0 {
    char pad[0x1f4];
    int m_1f4;
    char pad2[0x5d4 - 0x1f8];
    Sub4a08 m_5d4;
    char pad3[0x5f4 - 0x5d5];
    Sub4a08 m_5f4;
    char pad4[0x60c - 0x5f5];
    Sub4a08 m_60c;
    char pad5[0x6e8 - 0x60d];
    Sub4a08 m_6e8;
    char pad6[0x770 - 0x6e9];
    Sub4a08 m_770;
    int dispatch(unsigned flags, int a, int b, int c, int d);
};
// MATCH: golf_clean.exe 0x004a08a0 ?dispatch@C4a08a0@@QAEHIHHHH@Z
int C4a08a0::dispatch(unsigned flags, int a, int b, int c, int d)
{
    m_1f4 = flags & 0x1f;
    switch (m_1f4) {
    case 0x10:
        return m_5f4.handle(a, b, c, d);
    case 2:
        return m_5d4.handle(a, b, c, d);
    case 1:
        return m_60c.handle(a, b, c, d);
    case 8:
        return m_6e8.handle(a, b, c, d);
    case 4:
        return m_770.handle(a, b, c, d);
    }
    return 3;
}
