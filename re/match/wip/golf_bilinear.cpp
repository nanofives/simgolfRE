// golf_clean.exe 0x004674c0 (release). WIP 49%: same arithmetic (19-byte rows, weights (32-fx)/fx, (32-fy)/fy,
// sum / 32) but the original computes both offsets (x-0x80, y-0x80) first and keeps them in ecx/edx.
// FLAGS golf_clean.exe: /O2
extern signed char g_grid838c1c[];       // 0x838c1c, rows of 19 bytes (+1 = y+1, +19 = x+1, +20 = both)

// Bilinear interpolation over a 16x16 grid (256 units per cell, 32 sub-steps), offset by 0x80.
// MATCH: golf_clean.exe 0x004674c0 ?bilinear@@YAHHH@Z
int bilinear(int x, int y)
{
    x -= 0x80;
    y -= 0x80;
    int gx = x >> 8 & 0xf;
    int gy = y >> 8 & 0xf;
    int i = gy + gx * 19;
    int fx = x >> 3 & 0x1f;
    int fy = y >> 3 & 0x1f;
    return (g_grid838c1c[i + 1] * (32 - fx) * fy + g_grid838c1c[i + 19] * (32 - fy) * fx
            + g_grid838c1c[i] * (32 - fx) * (32 - fy) + g_grid838c1c[i + 20] * fy * fx) / 32;
}
