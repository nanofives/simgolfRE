// FLAGS golf_clean.exe: /O2 /GX
#include "golf_hand_07_hdr.h"
struct V1a49d5a0 { virtual void a0(); char pad[0x274 - 4]; };
struct V1b49d5a0 { virtual void b0(); char pad[0x300 - 4]; };
struct V149d5a0 : V1a49d5a0, V1b49d5a0 { V149d5a0(); ~V149d5a0(); };   // 0x4804a0
struct V249d5a0 { V249d5a0(); ~V249d5a0(); virtual void c0(); char pad[0x80 - 4]; int m_80; };   // 0x489150
extern char* g_4e4a10;
extern char* g_4e4a14;
extern char* g_4e4a18;
struct D49d5a0 : virtual V149d5a0, virtual V249d5a0 {
    D49d5a0();
    virtual void a0();
    virtual void c0();
    char* m_4;
    char* m_8;
    char* m_c;
    int m_10;
    int m_14;
    int m_18;
    int m_1c;
};
// MATCH: golf_clean.exe 0x0049d5a0 ??0D49d5a0@@QAE@XZ
D49d5a0::D49d5a0()
{
    m_10 = 0;
    m_14 = 0;
    m_8 = g_4e4a14;
    m_c = g_4e4a18;
    m_4 = g_4e4a10;
    m_18 = m_80;
    m_1c = m_80;
}
