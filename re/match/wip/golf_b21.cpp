// golf_clean.exe 0x00477fc0 / 0x00478140 / 0x00432f90 (release). WIP: logic as below, register roles differ.
// drawCentered/drawRight 49-52%: the original keeps s in ebx, this in ebp, n in esi (4 saved registers; ours 3).
//   Tried: MIN macro in an if, a separate len local with early returns.
// hitSlot 37%: the original walks the tables with pointers (0x4c79b6 shorts, 0x570cd4 rows), keeps y on the stack
//   (its register is reused for y - 0x242) and computes row.y - 10 before the x tests. Tried: a bottom local.
// Not in the 100% suite.
// FLAGS golf_clean.exe: /O2
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

struct FontRes { int m_0; int m_4; };
struct Font47 {
    char pad[0x5c]; FontRes* m_res;
    int width(const char* s, int n);                 // 0x477280
    int draw(const char* s, int x, int y, int n);    // 0x4775b0
    int drawCentered(const char* s, int x, int y, int n);
    int drawRight(const char* s, int x, int y, int n);
};
// Draws the first n characters centred on x; returns x for a null or empty string, 3 without a loaded font.
// MATCH: golf_clean.exe 0x00477fc0 ?drawCentered@Font47@@QAEHPBDHHH@Z
int Font47::drawCentered(const char* s, int x, int y, int n)
{
    if (!s)
        return x;
    if (!m_res || !m_res->m_4)
        return 3;
    if (MIN((int)strlen(s), n) >= 0) {
        n = MIN((int)strlen(s), n);
        if (n != 0)
            return draw(s, x - (width(s, n) >> 1), y, n);
    }
    return x;
}

// Same, right-aligned on x.
// MATCH: golf_clean.exe 0x00478140 ?drawRight@Font47@@QAEHPBDHHH@Z
int Font47::drawRight(const char* s, int x, int y, int n)
{
    if (!s)
        return x;
    if (!m_res || !m_res->m_4)
        return 3;
    if (MIN((int)strlen(s), n) >= 0) {
        n = MIN((int)strlen(s), n);
        if (n != 0)
            return draw(s, x - width(s, n), y, n);
    }
    return x;
}

int dist467170(int dx, int dy);              // 0x467170
extern signed char g_slotIds[];              // 0x4c79a0, -1 terminated
extern short g_slotPos[][2];                 // 0x4c79b4: x, y offset
struct Row570 { int y; char pad[0xb0 - 4]; };
extern Row570 g_rows570[];                   // 0x570cd4, stride 0xb0
// Item under (x, y): index of the slot box (0x3e x 0x2c) that contains it, else 0x15 / 0x16 near the two
// round buttons (radius 0xf at (0x105, 0x242), 0x14 at (0xed, 0x20d)), else -1.
// MATCH: golf_clean.exe 0x00432f90 ?hitSlot@@YAHHH@Z
int hitSlot(int x, int y)
{
    int hit = -1;
    int i = 0;
    if (dist467170(x - 0xed, y - 0x20d) < 0x14)
        hit = 0x16;
    if (dist467170(x - 0x105, y - 0x242) < 0xf)
        hit = 0x15;
    do {
        if (x >= g_slotPos[i][0] && x < g_slotPos[i][0] + 0x3e &&
            y >= g_rows570[i].y - 10 + g_slotPos[i][1] - 0x2c && g_rows570[i].y > y - 10 + g_slotPos[i][1])
            return i;
    } while (g_slotIds[++i] != -1);
    return hit;
}
