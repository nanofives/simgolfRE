// FLAGS golf_clean.exe: /O2 /GX
#include "golf_hand_07_hdr.h"
struct Rgb4826f0 { unsigned char r, g, b; };
struct Pal4826f0 { V4(a) virtual void get(Rgb4826f0*, int, int); virtual void set(Rgb4826f0*, int, int); };
struct Disp4826f0 { virtual void create(); Pal4826f0* m_pal; };
struct C4826f0 {
    char pad[0x74];
    Disp4826f0* m_74;
    int colorChunk(unsigned char* chunk);
};
// MATCH: golf_clean.exe 0x004826f0 ?colorChunk@C4826f0@@QAEHPAE@Z
int C4826f0::colorChunk(unsigned char* chunk)
{
    Rgb4826f0 pal[256];
    unsigned char c;
    unsigned short n;
    int count;
    if (!m_74)
        return 0;
    unsigned char* p = chunk + 6;
    if (m_74->m_pal)
        m_74->m_pal->get(pal, 0, 256);
    n = *(unsigned short*)p;
    p += 2;
    c = 0;
    while (n--) {
        c += *p++;
        count = *p++;
        if (count == 0)
            count = 256;
        while (count--) {
            pal[c].r = *p++;
            pal[c].g = *p++;
            pal[c].b = *p++;
            c++;
        }
    }
    Disp4826f0* d = m_74;
    if (!d->m_pal)
        d->create();
    d->m_pal->set(pal, 0, 256);
    return 0;
}
