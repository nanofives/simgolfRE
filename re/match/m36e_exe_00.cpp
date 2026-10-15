// golf_clean.exe 0x00477fc0 / 0x00478140 (release), round 36 batch m36e. Replaces re/match/wip/golf_b21.cpp's
// 48.8% / ~52% drafts of the same two functions (that file is left as it was).
// What differed there: (1) the draw call computed width() inside its argument list, so VC6 pushed n and y before
// calling width and kept n in a scratch register; the original calls width first (0x00478042 / 0x004781bd), keeps n
// in esi across that call and recomputes x from its stack slot afterwards: the source subtracts into x as its own
// statement (`x -= ...`; a temporary `int w = width(...)` compiles to 84.9%). (2) The original jumps from the
// MIN < 0 test (jl 0x478031) to the same inline `return x` block the n == 0 test skips with jne 0x47803c.
// Four spellings of that control flow compile identically (100%), so the source is NOT determined there:
//   if (MIN >= 0) { n = MIN; if (n == 0) return x; ... }  return x;          (used below)
//   if (MIN < 0) goto out; n = MIN; if (n == 0) goto out; ...  out: return x;
//   if (MIN >= 0) n = MIN; else n = 0; if (n == 0) return x; ...
//   if (MIN >= 0) { if ((n = MIN) == 0) return x; } else return x; ...
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
        if (n == 0)
            return x;
        x -= width(s, n) >> 1;
        return draw(s, x, y, n);
    }
    return x;
}

// Draws the first n characters ending at x.
// MATCH: golf_clean.exe 0x00478140 ?drawRight@Font47@@QAEHPBDHHH@Z
int Font47::drawRight(const char* s, int x, int y, int n)
{
    if (!s)
        return x;
    if (!m_res || !m_res->m_4)
        return 3;
    if (MIN((int)strlen(s), n) >= 0) {
        n = MIN((int)strlen(s), n);
        if (n == 0)
            return x;
        x -= width(s, n);
        return draw(s, x, y, n);
    }
    return x;
}
