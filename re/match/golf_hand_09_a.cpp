// FLAGS golf_clean.exe: /O2 /GX
#include <windows.h>
struct C492850 {
    C492850();
    ~C492850();
    virtual void v0();
    char pad[0x5c];
};
extern int g_4e4210;
extern int g_83ad44;
struct B474ae0 {
    B474ae0() { m_4 = 0; }
    virtual void b0();
    int m_4;
};
struct C474ae0 : B474ae0 {
    C474ae0();
    virtual void c0();
    int m_8, m_c, m_10, m_14, m_18, m_1c, m_20, m_24;
    char m_28;
    int m_2c, m_30, m_34, m_38, m_3c, m_40, m_44, m_48, m_4c, m_50, m_54, m_58, m_5c, m_60, m_64, m_68;
    int m_6c[4], m_7c[4], m_8c[4], m_9c[4];
    int m_ac;
    int m_b0;
    C492850 m_b4;
    int m_114[40];
    int m_1b4[40];
    C492850 m_254;
};
// MATCH: golf_clean.exe 0x00474ae0 ??0C474ae0@@QAE@XZ
C474ae0::C474ae0()
{
    int i;
    m_4 = 0;
    m_28 = 0;
    for (i = 0; i < 40; i++) {
        m_1b4[i] = 0;
        m_114[i] = -2;
    }
    m_ac = 0;
    m_8 = 0;
    m_10 = 0;
    m_c = 0;
    m_18 = 0;
    m_14 = 0;
    m_1c = 0;
    m_20 = 0;
    m_24 = 0;
    m_2c = -1;
    m_30 = 0;
    m_34 = 0;
    m_38 = 0;
    m_3c = 0;
    m_40 = g_4e4210;
    m_44 = -1;
    m_48 = 0;
    m_4c = 0;
    m_50 = 0;
    m_54 = 0;
    m_58 = 0;
    m_5c = g_83ad44;
    m_7c[0] = -1;
    m_8c[0] = 2;
    m_9c[0] = 2;
    m_6c[1] = -1;
    m_7c[1] = -1;
    m_8c[1] = 2;
    m_9c[1] = 2;
    m_6c[2] = -1;
    m_7c[2] = -1;
    m_8c[2] = 2;
    m_9c[2] = 2;
    m_6c[3] = -1;
    m_7c[3] = -1;
    m_8c[3] = 2;
    m_9c[3] = 2;
    m_6c[0] = 0;
    m_60 = 0;
    m_64 = 0;
    m_68 = 0;
}
