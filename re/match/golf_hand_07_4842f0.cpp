// FLAGS golf_clean.exe: /O2 /GX
#include <string.h>
#include "golf_hand_07_hdr.h"
struct S4842f0 { V4(a) virtual int load(const char*); V8(b) V4(c) virtual void c4(); virtual void loop(int); V4(d) virtual void d4(); virtual int busy(); };
extern int g_83afc4;
int open4840e0(S4842f0** pp, const char* name, int a);   // 0x4840e0
struct C4842f0 {
    V16(p) V8(q) V4(r) virtual void r4(); virtual void r5(); virtual void r6(); virtual void ready();
    char pad[0x30 - 4];
    int m_30;
    char pad2[0x40 - 0x34];
    S4842f0* m_40;
    unsigned m_44;
    char pad3[0x50 - 0x48];
    char* m_50;
    int setFile(const char* name);
};
// MATCH: golf_clean.exe 0x004842f0 ?setFile@C4842f0@@QAEHPBD@Z
int C4842f0::setFile(const char* name)
{
    int r;
    if (!g_83afc4)
        return 1;
    if (!m_40) {
        r = open4840e0(&m_40, name, 1);
        if (r)
            return r;
    } else if (m_40->busy())
        return 0xf;
    r = m_40->load(name);
    if (r == 0) {
        if (!(m_44 & 1)) {
            m_44 |= 1;
            ready();
            if (m_30)
                m_40->loop(1);
        }
    } else
        m_44 &= ~1;
    char* p = new char[strlen(name) + 1];
    strcpy(p, name);
    if (m_50)
        delete[] m_50;
    m_50 = p;
    return r;
}
