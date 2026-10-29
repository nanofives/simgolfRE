#include "jgldefs.h"
#include "DialogFrame.h"
#include "ShadeTable.h"
#include "Rgb555.h"
#include "GameMain.h"

unsigned short *g_shadeTable = new unsigned short[0x28000];

int ClampBrightnessAdd32(int v)
{
    if (v < 0xdf)
        return v + 0x20;
    return 0xff;
}

inline int PackRGB16(unsigned char r, unsigned char g, unsigned char b)
{
    if (graphsys->screen_mask() == 1)
        return (((0xfff00000 | r >> 3) << 6 | g >> 2) << 5) | b >> 3;
    return PackRgb555(r, g, b);
}

int Render_InitColorTable()
{
    unsigned short r, g, b;
    unsigned short c;

    for (r = 0; r < 0x100; r += 8)
        for (g = 0; g < 0x100; g += 8)
            for (b = 0; b < 0x100; b += 8) {
                c = PackRGB16(r, g, b);
                g_shadeTable[c] = PackRGB16(r * 128 >> 8, g * 128 >> 8, b * 128 >> 8);
            }
    for (r = 0; r < 0x100; r += 8)
        for (g = 0; g < 0x100; g += 8)
            for (b = 0; b < 0x100; b += 8) {
                c = PackRGB16(r, g, b);
                g_shadeTable[0x8000 + c] = PackRGB16(r * 160 >> 8, g * 160 >> 8, b * 160 >> 8);
            }
    for (r = 0; r < 0x100; r += 8)
        for (g = 0; g < 0x100; g += 8)
            for (b = 0; b < 0x100; b += 8) {
                c = PackRGB16(r, g, b);
                g_shadeTable[0x10000 + c] = PackRGB16(r * 192 >> 8, g * 192 >> 8, b * 192 >> 8);
            }
    for (r = 0; r < 0x100; r += 8)
        for (g = 0; g < 0x100; g += 8)
            for (b = 0; b < 0x100; b += 8) {
                c = PackRGB16(r, g, b);
                g_shadeTable[0x18000 + c] = PackRGB16(r * 224 >> 8, g * 224 >> 8, b * 224 >> 8);
            }
    for (r = 0; r < 0x100; r += 8)
        for (g = 0; g < 0x100; g += 8)
            for (b = 0; b < 0x100; b += 8) {
                c = PackRGB16(r, g, b);
                g_shadeTable[0x20000 + c] =
                    PackRGB16(ClampBrightnessAdd32(r), ClampBrightnessAdd32(g), ClampBrightnessAdd32(b));
            }
    return 0;
}

int g_shadeSpare[4] = {0};
