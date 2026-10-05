// FLAGS golf_clean.exe: /O2 /GX
// probe
struct Rng822d9c { int roll(int); };
extern Rng822d9c g_822d9c;
extern char g_838aa0[19][19];
extern char g_838c1c[17][19];
extern int g_4c2878[8];
extern int g_4c2898[8];
// MATCH: golf_clean.exe 0x004673e0 ?genNoise@@YAXXZ
void genNoise()
{
    int x, y, k;
    for (y = 0; y < 18; y++) {
        for (x = 0; x < 18; x++)
            g_838aa0[y][x] = g_822d9c.roll(16);
        g_838aa0[y][18] = g_838aa0[y][0];
    }
    for (x = 0; x < 18; x++)
        g_838aa0[18][x] = g_838aa0[0][x];
    for (y = 0; y <= 16; y++) {
        for (x = 0; x <= 16; x++) {
            int sum = g_838aa0[y][x] * 4;
            for (k = 0; k < 8; k++)
                sum += g_838aa0[y + g_4c2878[k] + 1][x + g_4c2898[k] + 1];
            g_838c1c[y][x] = g_838aa0[y][x];
        }
    }
    for (y = 0; y <= 16; y++)
        g_838c1c[y][16] = g_838c1c[y][0];
    for (x = 0; x <= 16; x++)
        g_838c1c[16][x] = g_838c1c[0][x];
}
