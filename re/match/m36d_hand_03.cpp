// FLAGS golf_clean.exe: /O2
// m36d: golf_clean.exe functions written by hand, batch 3 (release /O2). Names are placeholders; offsets, constants
// and callees are the original's. Callee conventions from their `ret N` (log/c3/m36d_notes.txt).
#include <windows.h>
#include <string.h>
#pragma vtordisp(off)
#define V4(a, b, c, d) virtual void v##a(); virtual void v##b(); virtual void v##c(); virtual void v##d();
#define V8(x) V4(x##0, x##1, x##2, x##3) V4(x##4, x##5, x##6, x##7)

// ---- 0x0047cce0 Window::shrinkRect: frame and title insets from the style word at +0x9c
extern int g_83ff10;
struct H3Rect { int l, t, r, b; void inflate(int d) { l -= d; r += d; t -= d; b += d; } };
struct H3Title { V8(a) V8(b) V8(c) V8(d) V8(e) V8(f) V8(g) V8(h) V8(i) V8(j) V8(k) V4(l0, l1, l2, l3) virtual int height(); };   // slot 92
struct H3Frame {
    char pad[0x9c];
    unsigned m_9c;
    char pada0[0x15c - 0xa0];
    H3Title* m_15c;
    char pad160[0x180 - 0x160];
    int m_180, m_184, m_188;
    void shrinkRect(H3Rect*);
};
// WIP (re/match/wip/m36d_wip_hand_03.cpp): golf_clean.exe 0x0047cce0 ?shrinkRect@H3Frame@@QAEXPAUH3Rect@@@Z
void H3Frame::shrinkRect(H3Rect* r)
{
    if (!r)
        return;
    if (m_9c & 4)
        r->b -= g_83ff10;
    if (m_9c & 8)
        r->r -= g_83ff10;
    if ((m_9c & 0x400) || (m_9c & 0x11)) {
        r->inflate(-m_184);
        if (m_188 != -1)
            r->b += m_184 - m_188;
    }
    if (m_9c & 0x10)
        r->t += m_180 - m_184;
    if (m_15c && !(m_9c & 0x20000000))
        r->t += m_15c->height();
}

// ---- 0x00492ed0 polyEdgeStep: next non-flat edge of the polygon from vertex `start` in direction e->dir
struct H3Pt { int x, y; };
struct H3Edge { int dir, h, idx, x, step, sign, err, adj, reset; };
extern int g_83d350;    // end vertex
extern int g_83d378;    // vertex count
extern H3Pt* g_83d358;  // vertices
// WIP (re/match/wip/m36d_wip_hand_03.cpp): golf_clean.exe 0x00492ed0 ?polyEdgeStep@@YAHPAUH3Edge@@H@Z
int polyEdgeStep(H3Edge* e, int i)
{
    while (i != g_83d350) {
        int next = i + e->dir;
        if (next > g_83d378 - 1)
            next = 0;
        else if (next < 0)
            next = g_83d378 - 1;
        e->h = g_83d358[next].y - g_83d358[i].y;
        if (e->h != 0) {
            if (e->h < 0)
                return 0;
            e->idx = next;
            e->x = g_83d358[i].x;
            int dx = g_83d358[next].x - g_83d358[i].x;
            if (dx < 0) {
                e->sign = -1;
                dx = -dx;
                e->err = 1 - e->h;
                e->step = -(dx / e->h);
            } else {
                e->sign = 1;
                e->step = dx / e->h;
                e->err = 0;
            }
            e->adj = dx % e->h;
            e->reset = e->h;
            return 1;
        }
        i = next;
    }
    return 0;
}

// ---- 0x00467600 pickGolferName: appends a golfer's name to the text buffer 0x0051a068
struct H3Walker { char pad[0x12]; unsigned char m_12; signed char m_13; char pad14[0x2a - 0x14]; char m_name[0x4c - 0x2a]; };
extern H3Walker g_585850[];
extern char g_51a068[];
extern char s_4d6098[];
extern char* g_4c148c[];
extern char* g_4c1464[];
// WIP (re/match/wip/m36d_wip_hand_03.cpp): golf_clean.exe 0x00467600 ?pickGolferName@@YAXH@Z
void pickGolferName(int i)
{
    H3Walker* w = &g_585850[i];
    if (w->m_name[0]) {
        strcat(g_51a068, w->m_name);
        return;
    }
    int k = ((w->m_12 & 8) >> 3) - (w->m_13 * 2 + 4);
    if (w->m_13 == -6) {
        strcat(g_51a068, s_4d6098);
        return;
    }
    strcat(g_51a068, g_4c148c[k]);
    strcat(g_51a068, g_4c1464[k]);
}

// ---- 0x0047ae80 setFocusItem: list of children at +0x138 (head) / +0x13c (cursor) / +0x140 (count) / +0x144 (index)
struct H3Item { V8(a) V8(b) V8(c) V8(d) V8(e) V8(f) V8(g) V4(h0, h1, h2, h3) virtual void setFocus(int); };   // slot 60
struct H3Node { int m_0; H3Item* m_4; int m_8; H3Node* m_c; };
extern H3Item* g_83ab98;
struct H3Focus {
    char pad[0x138];
    H3Node* m_head;
    H3Node* m_cur;
    int m_count;
    int m_index;
    H3Item* current() { return m_head ? m_cur->m_4 : 0; }
    void setFocusItem(H3Item*);
};
// WIP (re/match/wip/m36d_wip_hand_03.cpp): golf_clean.exe 0x0047ae80 ?setFocusItem@H3Focus@@QAEXPAUH3Item@@@Z
void H3Focus::setFocusItem(H3Item* item)
{
    if (!item || !m_count)
        return;
    H3Item* cur = current();
    if (item == cur)
        return;
    g_83ab98 = cur;
    if (!m_head)
        return;
    m_index = 0;
    m_cur = m_head;
    for (int i = 0; i < m_count; i++) {
        if (m_cur->m_4 == item) {
            if (cur && m_count > 1)
                cur->setFocus(0);
            if (current())
                current()->setFocus(1);
            return;
        }
        m_index++;
        m_cur = m_cur->m_c;
    }
}

// ---- text layout state: 0x00839aa0 y, 0x00839aa4 width, 0x00839aa8 x in the line
extern int g_839aa0, g_839aa4, g_839aa8, g_839a5c, g_83ad44;
extern char s_4c3f70[];
struct H3Text {
    char pad[0x14];
    int m_14, m_18, m_1c;
    char pad20[0x5c - 0x20];
    int m_5c;
    const char* wrap(const char*, int*, int);           // 0x00476ef0
    int drawText(const char*, int, int, int);            // 0x00477c30
    int lineHeight();                                    // 0x00477580
    int textWidth(const char*);                          // 0x00477250
    int measureText4(const char*);
    int drawTextLinesAt(const char*, int, int, int);
};
// WIP (re/match/wip/m36d_wip_hand_03.cpp): golf_clean.exe 0x00478610 ?measureText4@H3Text@@QAEHPBD@Z
int H3Text::measureText4(const char* text)
{
    if (!g_839aa4)
        return 0;
    if (!text)
        return g_839aa0;
    if (!m_5c)
        m_5c = g_83ad44;
    if (!*text)
        return g_839aa0;
    for (;;) {
        int avail = g_839aa4 - g_839aa8;
        const char* next = wrap(text, &avail, strlen(text));
        if (!next)
            break;
        m_1c++;
        g_839aa0 += lineHeight();
        g_839aa8 = 0;
        if (m_18) {
            g_839aa4 -= m_18;
            m_18 = 0;
        }
        text = next;
    }
    g_839aa8 += textWidth(s_4c3f70);
    return g_839aa0;
}
// WIP (re/match/wip/m36d_wip_hand_03.cpp): golf_clean.exe 0x00478430 ?drawTextLinesAt@H3Text@@QAEHPBDHHH@Z
int H3Text::drawTextLinesAt(const char* text, int x, int y, int w)
{
    if (!text || !w)
        return y;
    g_839aa8 = 0;
    m_1c = 0;
    m_18 = m_14;
    const char* next;
    do {
        int ww = w;
        if (!text) {
            next = 0;
            g_839a5c = x;
        } else {
            g_839aa8 = 0;
            next = wrap(text, &ww, strlen(text));
            if (next)
                g_839a5c = drawText(text, x, y, next - text);
            else
                g_839a5c = drawText(text, x, y, strlen(text));
        }
        y += lineHeight();
        m_1c++;
        if (m_18) {
            w -= m_18;
            x += m_18;
            m_18 = 0;
        }
        text = next;
    } while (next);
    return y;
}

// ---- 0x00488eb0 Button::endPress
struct H3Parent {
    V8(a) V8(b) V8(c) V8(d) V8(e) V8(f) V4(g0, g1, g2, g3) virtual void g4();
    virtual void onButton(int);     // slot 53 (0xd4)
    char pad[0x80 - 4];
    struct H3Hot { V4(0, 1, 2, 3) V4(4, 5, 6, 7) } *m_80;   // slot 7 (0x1c)
};
struct H3Radio { int select(int); /* 0x004a0350 */ };
extern H3Parent* g_83ab2c;
extern void (*g_83ab0c)();
extern void (*g_83ab10)();
struct H3Button {
    V4(0, 1, 2, 3) V4(4, 5, 6, 7) V8(a) V8(b) V8(c) V8(d) V8(e) V8(f) V8(g) V8(h)
    virtual void refresh();         // slot 72 (0x120)
    char pad4[0x130 - 4];
    H3Parent* m_130;
    char pad134[0x5e8 - 0x134];
    int m_5e8;                      // command id
    char pad5ec[0x614 - 0x5ec];
    int m_614;
    char pad618[0x620 - 0x618];
    H3Radio* m_620;
    int m_624;
    void (*m_628)(int);
    void endPress(int, int);
};
extern H3Button* g_83ab30;
extern H3Button* g_83ab34;
// MATCH: golf_clean.exe 0x00488eb0 ?endPress@H3Button@@QAEXHH@Z
void H3Button::endPress(int, int)
{
    if (g_83ab30 == this) {
        g_83ab30 = 0;
        v4();
    }
    if (g_83ab34 == this)
        g_83ab34 = 0;
    g_83ab2c = m_130;
    if (!m_624) {
        if (m_5e8 == -1) {
            if (g_83ab0c)
                g_83ab0c();
        } else if (m_5e8 == -2) {
            if (g_83ab10)
                g_83ab10();
        }
        if (m_130->m_80)
            m_130->m_80->v7();
        if (!m_620 || m_620->select(m_5e8))
            m_130->onButton(m_5e8);
    }
    if (m_628)
        m_628(m_5e8);
    if (m_614 == 1) {
        m_614 = 0;
        refresh();
    }
}

// ---- 0x0047bab0 Window::runModal: modal stack 0x0083a4e8 / 0x0083aa88 (depth 0x0083ab58, at most 4)
struct H3Modal;
extern int g_83ab58;
extern H3Modal* g_83a4e8[4];
extern H3Modal* g_83aa88[4];
extern H3Modal* g_83ab54;
extern H3Modal* g_83ab50;
extern H3Modal* g_83ab30m;
extern int g_83ab40, g_83ab44, g_83ab38;
void modalBegin483d30();    // 0x00483d30
void pumpInput483ba0();     // 0x00483ba0
struct H3Modal {
    virtual void v0();
    virtual void show(int);         // slot 1
    virtual void v2(); virtual void v3();
    virtual void release();         // slot 4
    V8(a) V8(b) V8(c) V8(d) V8(e) V8(f) V8(g) V4(h0, h1, h2, h3) virtual void h4(); virtual void h5(); virtual void h6();
    virtual void endModal();        // slot 68 (0x110)
    virtual int running();          // slot 69 (0x114)
    int runModal(int, int (*)(), H3Modal*);
};
// WIP (re/match/wip/m36d_wip_hand_03.cpp): golf_clean.exe 0x0047bab0 ?runModal@H3Modal@@QAEHHP6AHXZPAU1@@Z
int H3Modal::runModal(int flags, int (*idle)(), H3Modal* owner)
{
    int r = 0;
    if (g_83ab58 >= 4)
        return 0;
    g_83a4e8[g_83ab58] = this;
    g_83aa88[g_83ab58] = owner;
    g_83ab58++;
    g_83ab54 = this;
    g_83ab40 = 0;
    g_83ab44 = 0;
    if (g_83ab30m && g_83ab30m != this) {
        g_83ab30m->release();
        g_83ab30m = 0;
    }
    g_83ab38 = 0;
    if (owner) {
        g_83ab50 = owner;
        owner->show(0);
    }
    if (!(flags & 2))
        show(0);
    modalBegin483d30();
    SetCursor(LoadCursorA(0, IDC_ARROW));
    if (!(flags & 2) && g_83ab54 == this) {
        while (running()) {
            if (idle && !idle()) {
                if (g_83ab54)
                    g_83ab54->endModal();
                return 1;
            }
            pumpInput483ba0();
            if (g_83ab54 != this)
                return r;
        }
    }
    return r;
}

// ---- 0x00480a10 Window::paintBackground / 0x00480b30 Window::createBackingSurface
struct H3Surf {
    int m_0;
    void* m_4;
    int blit(H3Surf*, int, int, int, int, int, int);    // 0x00475c60
    void frame(int, int, int, int, int);                // 0x00475da0
    int fill(H3Rect*, int);                             // 0x00478b50
    int create(int, int, int, int, int, int);           // 0x00474dd0
};
struct H3WinFirst {
    V4(0, 1, 2, 3) V8(a) V8(b) virtual void b8x(); virtual void getSize(int*, int*, int*);   // slot 21 (0x54)
    V4(c0, c1, c2, c3) V8(d) V8(e) V8(f) V8(g) V8(h)
    virtual void j0(); virtual void j1(); virtual void j2(); virtual void j3(); virtual void j4();
    virtual int hasBackground();    // slot 71 (0x11c)
    char pad4[0x9c - 4];
    unsigned m_9c;
    char pada0[0x130 - 0xa0];
    struct H3Win* m_130;
    char pad134[0x1ac - 0x134];
    int m_1ac, m_1b0, m_1b4, m_1b8, m_1bc, m_1c0;
    char pad1c4[0x274 - 0x1c4];
};
struct H3Win : H3WinFirst, H3Surf {
    int paintBackground(H3Rect*, int);
    void createBackingSurface(int, int, int, int);
    void minSize(int*, int*);           // 0x0047ca10
    void clampSize(int*, int*);         // 0x0047cb10
    void resize(int, int, int, int);    // 0x0047b4e0
};
extern int g_83abe0, g_839ab0;
// WIP (re/match/wip/m36d_wip_hand_03.cpp): golf_clean.exe 0x00480a10 ?paintBackground@H3Win@@QAEHPAUH3Rect@@H@Z
int H3Win::paintBackground(H3Rect* r, int c)
{
    if ((m_9c & 0x80000) && m_130 && m_130->hasBackground()) {
        int ox = m_1bc + m_1ac;
        int oy = m_1c0 + m_1b0;
        H3Surf* src = this;
        int w = r->r - r->l;
        int h = r->b - r->t;
        m_130->blit(src, r->l + ox, r->t + oy, r->l, r->t, w, h);
        if (g_83abe0)
            frame(r->l, r->t, r->l + (r->r - r->l) - 1, r->t + (r->b - r->t) - 1, g_83abe0);
        return 0;
    }
    return fill(r, c);
}
// WIP (re/match/wip/m36d_wip_hand_03.cpp): golf_clean.exe 0x00480b30 ?createBackingSurface@H3Win@@QAEXHHHH@Z
void H3Win::createBackingSurface(int w, int h, int fixed, int depth)
{
    getSize(&w, &h, &fixed);
    if (fixed) {
        if (w <= 1)
            w = 1;
        if (h <= 1)
            h = 1;
    } else {
        int mw = 1, mh = 1;
        minSize(&mw, &mh);
        if (mw > w)
            w = mw;
        if (mh > h)
            h = mh;
    }
    if (!fixed)
        clampSize(&w, &h);
    g_839ab0 = 1;
    int r = create(w, h, depth, 0, (m_9c >> 28) & 8, 0);
    g_839ab0 = 0;
    if (!r)
        resize(w, h, 1, 0);
}

// ---- 0x004615f0 spreadShade: adds a falloff over the 15x15 tiles around (x, y) to one of two 50-wide byte grids
extern unsigned char g_82376c[][50];
extern unsigned char g_822da0[][50];
int blocked40bf60(int, int);    // 0x0040bf60
int approxDistance(int, int);   // 0x00467170
int clampInt(int, int, int);    // 0x00467130
// WIP (re/match/wip/m36d_wip_hand_03.cpp): golf_clean.exe 0x004615f0 ?spreadShade@@YAXHHH@Z
void spreadShade(int x, int y, int amount)
{
    int s = 2 * amount / 3;
    for (int i = -7; i <= 7; i++) {
        for (int j = -7; j <= 7; j++) {
            if (!blocked40bf60(x + i, y + j)) {
                int d = approxDistance(i * 3, j * 3) + 2;
                if (s > 0)
                    g_82376c[x + i][y + j] = clampInt(s / d + g_82376c[x + i][y + j], 0, 0xff);
                else
                    g_822da0[x + i][y + j] = clampInt(g_822da0[x + i][y + j] - s / d, 0, 0xff);
            }
        }
    }
}
