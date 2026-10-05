// FLAGS golf_clean.exe: /O2 /GX
// probe
int dist467170(int dx, int dy);
// MATCH: golf_clean.exe 0x00433c60 ?hit433c60@@YAHHH@Z
int hit433c60(int x, int y)
{
    int r = -1;
    if (dist467170(x - 0x10e, y - 0x1f3) < 16)
        r = -2;
    int y2 = y - 0x223;
    if (dist467170(x - 0x160, y2) < 25)
        r = 0;
    if (dist467170(x - 0x1d3, y2) < 25)
        r = 1;
    if (dist467170(x - 0x24b, y2) < 25)
        r = 2;
    if (dist467170(x - 0x2cb, y2) < 16)
        r = 3;
    if (dist467170(x - 0x10b, y - 0x231) < 12)
        return -3;
    return r;
}
struct Golfer579 { int m_x, m_y; int m_8; char pad[0x100 - 12]; };
extern Golfer579 g_5794b8[];
extern int g_561254, g_5a9cf0[], g_53df54, g_4c2ba0, g_4c2ba4, g_567afc, g_59ca54, g_5aa6c8;
int click436c00(int x, int y);
int pick435570(int x, int y);
// MATCH: golf_clean.exe 0x00435680 ?click435680@@YAHHH@Z
int click435680(int x, int y)
{
    if (g_561254)
        return click436c00(x, y);
    int r = pick435570(x, y);
    if (r >= 0) {
        int g = g_5a9cf0[r];
        if (g != -1) {
            if (g_5794b8[g].m_8 != -1 || g_53df54 > 0)
                g_53df54 = g + 1;
            g_4c2ba0 = g_5794b8[g].m_x >> 10;
            g_4c2ba4 = g_5794b8[g].m_y >> 10;
        }
    }
    if (r == -3) {
        g_567afc = 4;
        return -1;
    }
    if (r == -4) {
        g_59ca54 = 1;
        g_561254 = 1;
        return -1;
    }
    if (r == -5) {
        if (g_5aa6c8) {
            g_5aa6c8 -= 16;
            return -1;
        }
    } else if (r == -6) {
        g_5aa6c8 += 16;
    }
    return -1;
}
