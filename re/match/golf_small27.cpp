// golf_clean.exe functions, batch 27 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <string.h>

extern char g_text[];                    // 0x51a068
// Appends a golfer's opinion of the course for a rating 0..7 (8+ and negatives: the extreme lines; `twist`
// picks the alternative line at 6, 7 and 8+). The game's sentences (0x4e28e4 .. 0x4e2a74) are replaced by
// placeholders here: string operands are masked by the matcher and the text is game content.
// MATCH: golf_clean.exe 0x00469a20 ?appendOpinion@@YAXHH@Z
void appendOpinion(int rating, int twist)
{
    switch (rating) {
    case 0: strcat(g_text, "<0x4e2a74>"); break;
    case 1: strcat(g_text, "<0x4e2a54>"); break;
    case 2: strcat(g_text, "<0x4e2a34>"); break;
    case 3: strcat(g_text, "<0x4e2a14>"); break;
    case 4: strcat(g_text, "<0x4e29f0>"); break;
    case 5: strcat(g_text, "<0x4e29d4>"); break;
    case 6:
        if (twist <= 1)
            strcat(g_text, "<0x4e29b4>");
        else
            strcat(g_text, "<0x4e2990>");
        break;
    case 7:
        if (twist < 1)
            strcat(g_text, "<0x4e2974>");
        else
            strcat(g_text, "<0x4e2958>");
        break;
    default:
        if (rating < 0 || rating > 0x7f)
            strcat(g_text, "<0x4e28e4>");
        else if (twist < 1)
            strcat(g_text, "<0x4e2930>");
        else
            strcat(g_text, "<0x4e2908>");
        break;
    }
}

extern unsigned int g_flags;             // 0x59e7b8
int worldToScreen(int wx, int wy, int* sx, int* sy, int flags);   // 0x42fb90
void drawText404(const char* s, int x, int y, int color);        // 0x404bc0
void appendMoney(int cents);             // 0x42dc00
struct Font476 { void select(void* style, int a, int b, int c); }; // 0x4762d0
extern Font476* g_font4c1570;            // 0x4c1570
extern char g_style51b360[];             // 0x51b360
extern int g_moneyTime[8];               // 0x542f00
extern int g_moneyX[8];                  // 0x542fd8
extern int g_moneyY[8];                  // 0x542ff8
extern int g_moneyAmt[8];                // 0x542dd8
// Draws the floating money labels ("+$n" for income), counting their time down; hidden under flag 0x1000000.
// MATCH: golf_clean.exe 0x0040c910 ?drawMoneyLabels@@YAXXZ
void drawMoneyLabels()
{
    int sx, sy;
    for (int i = 0; i < 8; i++) {
        if (g_moneyTime[i] && !(g_flags & 0x1000000)) {
            g_moneyTime[i]--;
            if (worldToScreen(g_moneyX[i], g_moneyY[i], &sx, &sy, 0)) {
                int amt = g_moneyAmt[i];
                strcpy(g_text, amt > 0 ? "+" : "");
                appendMoney(amt * 100);
                g_font4c1570->select(g_style51b360, 0, 0, 0);
                drawText404(g_text, sx, sy - 4, g_moneyAmt[i] > 0 ? 0x800023e8 : 0x80007d08);
            }
        }
    }
}

int shotReach4223f0(int power, int lie);    // 0x4223f0
int puttReach4223c0(int power);             // 0x4223c0
extern int g_cacheDist[10];              // 0x5a47b8
extern int g_cacheLie[10];               // 0x5685c8
extern int g_cacheVal[10];               // 0x53fd20
extern int g_cacheNext;                  // 0x5a9ce0
// Shot power that reaches `dist` tiles (binary search over the reach function from a quadratic first guess),
// with a 10-entry ring cache keyed on (dist, lie).
// MATCH: golf_clean.exe 0x00422430 ?powerFor@@YAHHHH@Z
int powerFor(int dist, int lie, int putt)
{
    for (int i = 0; i < 10; i++)
        if (dist == g_cacheDist[i] && lie == g_cacheLie[i])
            return g_cacheVal[i];
    g_cacheDist[g_cacheNext] = dist;
    g_cacheLie[g_cacheNext] = lie;
    int t = dist * 20 / 25;
    int v = t * 33 - t * t / 48 + 64;
    int step = v / 2;
    int target = (dist << 10) / 25;
    do {
        int r = !putt ? shotReach4223f0(v, lie) : puttReach4223c0(v);
        if (r > target)
            v -= step;
        if (r < target)
            v += step;
        step /= 2;
    } while (step > 2);
    g_cacheVal[g_cacheNext] = v;
    g_cacheNext = (g_cacheNext + 1) % 10;
    return v;
}

extern short g_tileSX[50][50];           // 0x55eb40 (0: not cached, -99: off screen)
extern short g_tileSY[50][50];           // 0x55fec8
extern int g_viewW, g_viewH;             // 0x822c8c, 0x822c90
// Screen position of the centre of tile (tx, ty), cached per tile; 0 when off the view (with margins).
// MATCH: golf_clean.exe 0x0042f940 ?tileToScreen@@YAHHHPAH0@Z
int tileToScreen(int tx, int ty, int* sx, int* sy)
{
    if (tx >= 0 && tx < 50 && ty >= 0 && ty < 50) {
        short c = g_tileSX[tx][ty];
        if (c) {
            if (c != -99) {
                *sx = c;
                *sy = g_tileSY[tx][ty];
                return 1;
            }
        } else {
            worldToScreen(tx * 0x400 + 0x200, ty * 0x400 + 0x200, sx, sy, 0);
            if (*sx >= -0x40 && *sx < g_viewW + 0x40 && *sy >= -0x2a && *sy < g_viewH + 0x2a) {
                g_tileSX[tx][ty] = (short)*sx;
                g_tileSY[tx][ty] = (short)*sy;
                return 1;
            }
            g_tileSX[tx][ty] = -99;
        }
    }
    return 0;
}
