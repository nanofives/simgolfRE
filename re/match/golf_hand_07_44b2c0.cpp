// FLAGS golf_clean.exe: /O2 /GX
#include "golf_hand_07_hdr.h"
struct M44b2c0 { M44b2c0(); ~M44b2c0(); char pad[0x2c]; };   // ctor 0x473ab0, dtor 0x4041f0
struct C44b2c0 {
    C44b2c0();
    M44b2c0 m_0, m_2c, m_58, m_84, m_b0, m_dc, m_108, m_134;
    M44b2c0 m_160[8];
    M44b2c0 m_2c0[0x24];
};
// MATCH: golf_clean.exe 0x0044b2c0 ??0C44b2c0@@QAE@XZ
C44b2c0::C44b2c0()
{
}
