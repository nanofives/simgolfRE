// FLAGS golf_clean.exe: /O2 /GX
#include "golf_hand_07_hdr.h"
struct M44b8d0 { M44b8d0(); ~M44b8d0(); char pad[0x2c]; };   // ctor 0x473ab0
struct C44b8d0 {
    C44b8d0();
    M44b8d0 m_0, m_2c, m_58, m_84, m_b0, m_dc, m_108, m_134, m_160, m_18c, m_1b8, m_1e4;
};
// MATCH: golf_clean.exe 0x0044b8d0 ??0C44b8d0@@QAE@XZ
C44b8d0::C44b8d0()
{
}
