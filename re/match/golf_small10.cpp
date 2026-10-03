// Small golf_clean.exe functions, batch 10 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <string.h>

class Snd484 {
public:
    int  setName(const char* s);         // 0x484b80
    void setMode(int m);                 // 0x484260
    char pad[0x44];
    unsigned int m_44;                   // +0x44 flags
    char pad48[0x50 - 0x48];
    char* m_50;                          // +0x50 name
    int   m_54;                          // +0x54 mode
};

// MATCH: golf_clean.exe 0x00484b80 ?setName@Snd484@@QAEHPBD@Z
int Snd484::setName(const char* s)
{
    if (s) {
        if (m_50)
            delete[] m_50;
        m_50 = new char[strlen(s) + 1];
        strcpy(m_50, s);
        return 0;
    }
    return 10;
}

// Case blocks laid out 4, 2, 1, 5, 6, 7 (jump table 0x4842d0). Mode 3 and out-of-range values only store the
// mode: the default target 0x4842c7 is the `m_54 = m` store that case 7 falls into.
// MATCH: golf_clean.exe 0x00484260 ?setMode@Snd484@@QAEXH@Z
void Snd484::setMode(int m)
{
    switch (m) {
    case 4: m_44 |= 0x10; m_54 = m; break;
    case 2: m_44 |= 8; m_54 = m; break;
    case 1: m_44 |= 4; m_54 = m; break;
    case 5: m_44 |= 0x28; m_54 = m; break;
    case 6: m_44 |= 0x100; m_54 = m; break;
    case 7: m_44 |= 0x80;
    default: m_54 = m;
    }
}

extern short g_spots[];                  // 0x4c7930: (x, y) pairs ended by -1
int approxDistance(int dx, int dy);      // 0x467170

// Index of the nearest spot, distance weighted by (index + 6) / 8 and below 30; spot 6 counts as none.
// MATCH: golf_clean.exe 0x004326a0 ?nearestSpot@@YAHHH@Z
int nearestSpot(int x, int y)
{
    int best = -1;
    int i = 0;
    int bestD = 30;
    do {
        int d = approxDistance(x - g_spots[i * 2], y - g_spots[i * 2 + 1]) * (i + 6) / 8;
        if (d < bestD) {
            bestD = d;
            best = i;
        }
        i++;
    } while (g_spots[i * 2] != -1);
    if (best == 6)
        best = -1;
    return best;
}

extern unsigned int g_flags;             // 0x59e7b8
struct HoleStat { int v; char pad[0x208 - 4]; };
extern HoleStat g_575ca0[];              // 0x575ca0, stride 0x208
extern int g_popPts[8], g_popX[8], g_popY[8], g_popTtl[8];   // 0x542dd8, 0x542fd8, 0x542ff8, 0x542f00
extern int g_popNext;                    // 0x59abb0

// Queues a points popup (8-entry ring, life 0x18) and subtracts the points from entry `who` unless -1.
// MATCH: golf_clean.exe 0x0040c890 ?pointsPopup@@YAXHHHH@Z
void pointsPopup(int pts, int x, int y, int who)
{
    if (!pts || (g_flags & 0x1000000))
        return;
    if (who != -1)
        g_575ca0[who].v -= pts;
    g_popX[g_popNext] = x;
    g_popY[g_popNext] = y;
    g_popPts[g_popNext] = pts;
    g_popTtl[g_popNext] = 0x18;
    g_popNext = (g_popNext + 1) % 8;
}

struct Rec3c { short id; char pad[0x3a]; };
extern Rec3c g_rec3c[100];               // 0x56d1d8
void clearRec74(int i);                  // 0x401000
void f_401040(int a, int b, int c, int d);   // 0x401040

// The calls for 0, 1 and 2 are separate in the binary, then a loop for 3..5.
// MATCH: golf_clean.exe 0x00401750 ?initRecords@@YAXXZ
void initRecords()
{
    for (int i = 0; i < 100; i++)
        g_rec3c[i].id = -1;
    clearRec74(0);
    f_401040(0, 0, 0, 0);
    clearRec74(1);
    f_401040(1, 1, 0, 0);
    clearRec74(2);
    f_401040(2, 2, 0, 0);
    for (int j = 3; j < 6; j++) {
        clearRec74(j);
        f_401040(j, j, 0, 0);
    }
}
