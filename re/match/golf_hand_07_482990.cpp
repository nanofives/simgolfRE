// FLAGS golf_clean.exe: /O2 /GX
#include <string.h>
#include "golf_hand_07_hdr.h"
struct Surf482990 { V8(a) virtual char* lock(); virtual void unlock(int); };
struct Disp482990 { int m_0; Surf482990* m_surf; };
struct Hdr482990 { char pad[8]; unsigned short width, height; };
struct C482990 {
    char pad[0x58];
    Hdr482990* m_58;
    int brun(unsigned char* chunk, int unused, Disp482990* d);
};
// MATCH: golf_clean.exe 0x00482990 ?brun@C482990@@QAEHPAEHPAUDisp482990@@@Z
int C482990::brun(unsigned char* chunk, int unused, Disp482990* d)
{
    char* p = (char*)chunk + 6;
    char* dst = d->m_surf->lock();
    int lines = m_58->height;
    while (lines--) {
        p++;
        int w = m_58->width;
        while (w) {
            char n = *p++;
            if (n < 0) {
                n = -n;
                memcpy(dst, p, n);
                dst += n;
                p += n;
            } else {
                memset(dst, *p++, n);
                dst += n;
            }
            w -= n;
        }
    }
    d->m_surf->unlock(1);
    return 0;
}
