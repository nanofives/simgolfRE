// Small golf_clean.exe functions, batch 15 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <string.h>

// Price by tier 0..15 (500 .. 10000; out of range 500), then * 100 / 100 as compiled (lea/shl then /100).
// MATCH: golf_clean.exe 0x0046f1d0 ?tierPrice@@YAHH@Z
int tierPrice(int tier)
{
    int v = 500;
    switch (tier) {
    case 0: v = 500; break;
    case 1: v = 600; break;
    case 2: v = 700; break;
    case 3: v = 800; break;
    case 4: v = 1200; break;
    case 5: v = 1500; break;
    case 6: v = 2000; break;
    case 7: v = 2500; break;
    case 8: v = 3000; break;
    case 9: v = 4000; break;
    case 10: v = 5000; break;
    case 11: v = 6000; break;
    case 12: v = 7000; break;
    case 13: v = 8000; break;
    case 14: v = 9000; break;
    case 15: v = 10000; break;
    }
    return v * 100 / 100;
}

extern char g_text[];                    // 0x51a068

// Appends the club name for 0..13 (Driver .. Putter, strings 0x4c5220 .. 0x4c51b0) to the text buffer. Each case
// calls strcat; VC6 merges the calls into one block that takes the string in edi (case strings checked by hand
// against the jump table at 0x40aa44).
// MATCH: golf_clean.exe 0x0040a9a0 ?appendClubName@@YAXH@Z
void appendClubName(int club)
{
    switch (club) {
    case 0: strcat(g_text, "Driver"); break;
    case 1: strcat(g_text, "3 Wood"); break;
    case 2: strcat(g_text, "4 Wood"); break;
    case 3: strcat(g_text, "2 Iron"); break;
    case 4: strcat(g_text, "3 Iron"); break;
    case 5: strcat(g_text, "4 Iron"); break;
    case 6: strcat(g_text, "5 Iron"); break;
    case 7: strcat(g_text, "6 Iron"); break;
    case 8: strcat(g_text, "7 Iron"); break;
    case 9: strcat(g_text, "8 Iron"); break;
    case 10: strcat(g_text, "9 Iron"); break;
    case 11: strcat(g_text, "Lob Wedge"); break;
    case 12: strcat(g_text, "Sand Wedge"); break;
    case 13: strcat(g_text, "Putter"); break;
    }
}

struct Rect4 { int x0, y0, x1, y1; };
int inRect(int x, int y, Rect4* r);      // 0x492610
struct Hot20 { int id; Rect4 r; int b; int a; int pad; };

class HotList2 {
public:
    int hitRect(int x, int y, int* a, int* b, Rect4* out);   // 0x492b10
    char pad[0x50];
    Hot20* m_50; int pad54; int m_58;
};

// Like HotList::hit (0x492a90) but also copies the entry's rect.
// MATCH: golf_clean.exe 0x00492b10 ?hitRect@HotList2@@QAEHHHPAH0PAURect4@@@Z
int HotList2::hitRect(int x, int y, int* a, int* b, Rect4* out)
{
    for (int i = m_58 - 1; i >= 0; i--) {
        if (inRect(x, y, &m_50[i].r)) {
            if (a)
                *a = m_50[i].a;
            if (b)
                *b = m_50[i].b;
            if (out)
                *out = m_50[i].r;
            return i;
        }
    }
    return -1;
}
