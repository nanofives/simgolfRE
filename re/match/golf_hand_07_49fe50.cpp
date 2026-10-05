// FLAGS golf_clean.exe: /O2 /GX
#include "golf_hand_07_hdr.h"
struct Img49fe50 {
    Img49fe50();     // 0x474ae0
    ~Img49fe50();    // 0x474c40
    int load(const char* name, void* p, int a, int b, int c);   // 0x475840
    char pad[0x2b8];
};
struct Font49fe50 { void cut(Img49fe50* img, int a, int b, int w, int h, int c, int d); char pad[0x30]; };  // 0x473bf0
extern Font49fe50 g_8408c8;
extern Font49fe50 g_8408f8;
extern const char s_004e49fc[];
// MATCH: golf_clean.exe 0x0049fe50 ?loadFonts49fe50@@YAHXZ
int loadFonts49fe50()
{
    Img49fe50 img;
    int r = img.load(s_004e49fc, 0, 0, 0x100, 2);
    if (r)
        return r;
    g_8408c8.cut(&img, 1, 0x23, 0x20, 0x20, 1, 1);
    g_8408f8.cut(&img, 0x22, 0x23, 0x20, 0x20, 1, 1);
    return 0;
}
