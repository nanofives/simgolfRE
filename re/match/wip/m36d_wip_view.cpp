// FLAGS golf_clean.exe: /O2 /GX
// m36d near misses (round 36): copy of re/match/m36d_view.cpp whose MATCH lines are only the functions below
// 100%. Kept in wip/ so the 100% test and the evidence run never see them; what was tried is in
// log/c3/m36d_notes.txt.
// m36d: the composite view class (ctor 0x004a0740, dtor 0x00491500; golf_hand_r0.cpp R0D4a0740) and four of its
// bases, which share two virtual bases: the window (dtor 0x004805a0, 0x574 bytes, Surface part at +0x274) and a second
// one (dtor 0x00489370). Offsets come from the original's code:
//   ListBox  at view+0x000: window at +0x5c, second vbase at +0x5d4 (??_G 0x0048cba0), vfptr at +0, vbptr at +4
//   listPanel at view+0x058: window at +0x24, second at +0x59c (??_G 0x00491ad0)
//   optionDialog view+0x078: window at +0x1c, second at +0x594 (??_G 0x0049ff60)
//   panelSub  at view+0x090: window at +0xe0, second at +0x658 (??_G 0x004a4980)
//   panelRow  at view+0x16c: window at +0x8c, second at +0x604 (??_G 0x004a3050), vbptr at +0
//   view: kind word at +0x1f4 (window-relative [ecx-8], second-vbase-relative [ecx-0x580]), window at +0x1fc,
//   second vbase at +0x774 (??_G 0x00491900).
// Overriders of a virtual-base virtual take `this` as that base's static position in the overriding class, so a
// qualified call from the view to a base's overrider adjusts by a constant (ListBox -0x1a0, listPanel -0x180,
// optionDialog -0x168, panelSub -0x8c, panelRow -4). The dispatchers switch on the kind word with the case blocks in
// the order 16, 2, 1, 8, 4 (the jump tables are compared by match.py, so which empty cases exist is fixed too).
// Virtual names are placeholders taken from the rows (viewDispatchSlotNN = the window vtable slot the row names);
// the called slots 30, 38, 58 and 72 sit at their offsets 0x78, 0x98, 0xe8 and 0x120.
#define W4(a, b, c, d) virtual void w##a(); virtual void w##b(); virtual void w##c(); virtual void w##d();

extern int g_83b654;
extern int g_83ab2c;
extern int g_83ff18;

struct M36Item { char pad[0x9c]; unsigned m_9c; };   // build's first argument
struct M36Image {   // Surface image: slots 54 / 55 (0xd8 / 0xdc) = width / height (golf_hand_r0.cpp R0I4796a0)
    W4(0, 1, 2, 3) W4(4, 5, 6, 7) W4(8, 9, 10, 11) W4(12, 13, 14, 15) W4(16, 17, 18, 19) W4(20, 21, 22, 23)
    W4(24, 25, 26, 27) W4(28, 29, 30, 31) W4(32, 33, 34, 35) W4(36, 37, 38, 39) W4(40, 41, 42, 43) W4(44, 45, 46, 47)
    W4(48, 49, 50, 51) virtual void w52(); virtual void w53();
    virtual int width(); virtual int height();
};
struct M36Target {  // the object at [window+0x13c]+4: slot 74 (0x128)
    W4(0, 1, 2, 3) W4(4, 5, 6, 7) W4(8, 9, 10, 11) W4(12, 13, 14, 15) W4(16, 17, 18, 19) W4(20, 21, 22, 23)
    W4(24, 25, 26, 27) W4(28, 29, 30, 31) W4(32, 33, 34, 35) W4(36, 37, 38, 39) W4(40, 41, 42, 43) W4(44, 45, 46, 47)
    W4(48, 49, 50, 51) W4(52, 53, 54, 55) W4(56, 57, 58, 59) W4(60, 61, 62, 63) W4(64, 65, 66, 67) W4(68, 69, 70, 71)
    virtual void w72(); virtual void w73(); virtual void slot74();
};
struct M36Holder { int m_0; M36Target* m_4; };

#pragma vtordisp(on)
struct M36WinSurface {   // the Surface part of every window, at +0x274
    virtual void s0();
    M36Image* m_4;
    int width() { return m_4 ? m_4->width() : 0; }
    int height() { return m_4 ? m_4->height() : 0; }
    int blit(M36WinSurface* src, int x, int y, int sx, int sy, int w, int h);   // 0x00475c60
};
struct M36Win;
struct M36WinFirst {
    virtual ~M36WinFirst();
    virtual void slot1(); virtual void slot2(); virtual void slot3(); virtual void slot4(); virtual void slot5();
    virtual int slot6();
    virtual int slot7(int);
    virtual int slot8(); virtual int slot9(int); virtual void w10(); virtual void w11();
    W4(12, 13, 14, 15) W4(16, 17, 18, 19) W4(20, 21, 22, 23)
    virtual void draw();                        // 24
    virtual void scrollBy(int, int, int);       // 25
    virtual void w26();
    virtual void slot27(int, int);
    virtual void slot28(int, int);
    virtual void slot29(int, int);
    virtual void slot30(int, int);
    virtual void slot31(int, int);
    virtual void slot32(int, int);
    virtual void slot33(int, int);
    virtual void slot34(int, int);
    virtual void w35();
    virtual int slot36(int);
    virtual void w37();
    virtual void selectChanged(int, int);       // 38
    virtual void slot39(int, int);
    W4(40, 41, 42, 43) W4(44, 45, 46, 47) W4(48, 49, 50, 51) W4(52, 53, 54, 55)
    virtual void w56();
    virtual void onScroll(int, int);            // 57
    virtual void onScroll58(int, int);          // 58
    virtual void w59();
    virtual void slot60(int);
    W4(61, 62, 63, 64) W4(65, 66, 67, 68) virtual void w69(); virtual void w70(); virtual void w71();
    virtual void refresh();                     // 72
    virtual void cycleSelection();              // 0x0047af60
    virtual void fwdOnScroll(int, int);
    virtual void onNavKeyMarked(int, int);
    virtual void fireSelectChangedMarked(int, int);
    int create(int, int, int, int, int, int, M36Item*, int, int);   // 0x004806c0
    void invalidate();                                              // 0x00480ce0
    char pad4[0x9c - 4];
    unsigned char m_9c;
    char pad9d[0x130 - 0x9d];
    M36Win* m_130;      // the parent window
    int m_134;
    int m_138;
    M36Holder* m_13c;
    int m_140;
    char pad144[0x274 - 0x144];
};
struct M36Win : M36WinFirst, M36WinSurface { char pad[0x574 - 0x27c]; };
struct M36Second {   // ListModel
    virtual ~M36Second();
    virtual int build(M36Item*, int, int, int);
    int findId(int);    // 0x004899d0
    int idAt(int);      // 0x00489a30
    char pad4[0x28 - 4];
    int m_28, m_2c;
    char pad30[0x48 - 0x30];
    int m_48;
    char pad4c[0x5c - 0x4c];
    int m_5c, m_60, m_64, m_68, m_6c;
    char pad70[0xf0 - 0x70];
    int m_f0;
};

// ---- ListBox (dtor 0x00489de0)
struct M36ListBox : virtual M36Win, virtual M36Second {
    virtual void own(int);
    char pad8[0x28 - 8];
    int m_28;           // scroll position
    char pad2c[0x58 - 0x2c];
    void setSelection(int, int);    // 0x00489f50
    virtual ~M36ListBox();
    virtual int build(M36Item*, int, int, int);
    virtual int slot6() { if ((m_9c & 0x40) && m_130) return m_130->slot8(); return 0; }
    virtual int slot7(int a) { if ((m_9c & 0x40) && m_130) return m_130->slot9(a); return a; }
    virtual void draw();
    virtual void scrollBy(int, int, int);
    virtual void slot27(int, int);
    virtual void slot28(int, int);
    virtual void slot29(int, int);
    virtual void slot30(int, int);
    virtual int slot36(int);
    virtual void selectChanged(int, int);
    virtual void onScroll(int, int);
    virtual void slot60(int);
    virtual void fwdOnScroll(int, int);
    virtual void onNavKeyMarked(int, int);
    virtual void fireSelectChangedMarked(int, int);
};
// ---- listPanel (dtor body inlined in its ??_G: cleanup 0x0049d690)
struct M36ListPanel : virtual M36Win, virtual M36Second {
    char pad4[0x20 - 4];
    ~M36ListPanel() { cleanup(); }
    void cleanup();                             // 0x0049d690
    int layoutText();                           // 0x0049d770
    virtual int build(M36Item*, int, int, int);
    virtual void draw();
    virtual void slot27(int, int);
    virtual void slot28(int, int);
    virtual void slot30(int, int);
    virtual int slot36(int);
    virtual void selectChanged(int, int);
    virtual void slot60(int);
};
// ---- optionDialog (dtor 0x0049ec80)
struct M36OptionDialog : virtual M36Win, virtual M36Second {
    int m_4;                                    // current list index
    void (*m_8)(int);                           // change callback
    char padc[0x18 - 0xc];
    void toggle(int);                           // 0x0049ee90
    int bitTest(int);                           // 0x0049f030
    int flagIsSet(int);                         // 0x0049ef80
    void drawItem(int, int, int);               // 0x0049f370
    virtual ~M36OptionDialog();
    virtual int build(M36Item*, int, int, int);
    virtual void draw();
    virtual void slot27(int, int);
    virtual void slot28(int, int);
    virtual void slot30(int, int);
    virtual void slot31(int, int) {}
    virtual int slot36(int);
    virtual void selectChanged(int, int);
    virtual void slot60(int);
};
// ---- panelSub (dtor 0x004a3480)
struct M36PanelSub : virtual M36Win, virtual M36Second {
    char pad4[0x88 - 4];
    int m_88;                                   // item count
    char pad8c[0xdc - 0x8c];
    void drawItem(int);                         // 0x004a3f10
    virtual ~M36PanelSub();
    virtual int build(M36Item*, int, int, int);
    virtual void draw();
    virtual void slot27(int, int);
    virtual void slot28(int, int);
    virtual void slot29(int, int);
    virtual void slot30(int, int);
    virtual void slot31(int, int);
    virtual void slot32(int, int);
    virtual void slot33(int, int);
    virtual void slot34(int, int);
    virtual int slot36(int);
    virtual void selectChanged(int, int);
    virtual void slot39(int, int);
    virtual void slot60(int);
};
// ---- panelRow (dtor 0x004a2330)
struct M36PanelRow : virtual M36Win, virtual M36Second {
    char pad4[0x88 - 4];
    virtual ~M36PanelRow();
    virtual int build(M36Item*, int, int, int);
    virtual void draw();
    virtual void slot60(int);
    virtual void cycleSelection();
};
// ---- the view (ctor 0x004a0740)
struct M36View : M36ListBox, M36ListPanel, M36OptionDialog, M36PanelSub, M36PanelRow {
    int m_kind;
    virtual ~M36View();
    virtual int build(M36Item*, int, int, int);
    virtual int slot6();
    virtual int slot7(int);
    virtual void draw();
    virtual void scrollBy(int, int, int);
    virtual void slot27(int, int);
    virtual void slot28(int, int);
    virtual void slot29(int, int);
    virtual void slot30(int, int);
    virtual void slot31(int, int);
    virtual void slot32(int, int);
    virtual void slot33(int, int);
    virtual void slot34(int, int);
    virtual int slot36(int);
    virtual void selectChanged(int, int);
    virtual void slot39(int, int);
    virtual void onScroll(int, int);
    virtual void slot60(int);
    virtual void cycleSelection();
};

// The destructors are defined here (empty) only so that VC6 emits the vtables and the ??_G deleting destructors; their
// own bodies are other functions of the original (listed with each class) and carry no MATCH.
M36ListBox::~M36ListBox() {}
M36OptionDialog::~M36OptionDialog() {}
M36PanelSub::~M36PanelSub() {}
M36PanelRow::~M36PanelRow() {}
M36View::~M36View() {}

// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x0048cba0 ??_GM36ListBox@@UAEPAXI@Z
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x0048cbf0 ?slot6@M36ListBox@@UAEHXZ
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x0048cc30 ?slot7@M36ListBox@@UAEHH@Z
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x0048c4f0 ?fwdOnScroll@M36ListBox@@UAEXHH@Z
void M36ListBox::fwdOnScroll(int a, int b)
{
    onScroll58(a, b);
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x0048a9c0 ?onNavKeyMarked@M36ListBox@@UAEXHH@Z
void M36ListBox::onNavKeyMarked(int a, int b)
{
    g_83b654 = 1;
    slot30(a, b);
    g_83b654 = 0;
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x0048c520 ?fireSelectChangedMarked@M36ListBox@@UAEXHH@Z
void M36ListBox::fireSelectChangedMarked(int a, int b)
{
    g_83b654 = 1;
    selectChanged(a, b);
    g_83b654 = 0;
}

// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x0048c420 ?onScroll@M36ListBox@@UAEXHH@Z
void M36ListBox::onScroll(int how, int v)
{
    g_83ab2c = (int)m_130;
    int old = m_28;
    int oldSel = m_f0;
    switch (how) {
    case -1: m_28 = v; break;
    case -2: m_28 = m_5c * v; break;
    }
    if (g_83ff18)
        setSelection(m_28, 1);
    if (m_28 != old) {
        refresh();
        return;
    }
    if (oldSel != m_f0) {
        own(oldSel);
        own(m_f0);
    }
    invalidate();
}

// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x00491ad0 ??_GM36ListPanel@@UAEPAXI@Z
// MATCH: golf_clean.exe 0x0049d6d0 ?build@M36ListPanel@@UAEHPAUM36Item@@HHH@Z
int M36ListPanel::build(M36Item* item, int x, int y, int flags)
{
    if (!item)
        return 3;
    int r = layoutText();
    if (r)
        return r;
    m_28 = x;
    m_2c = y;
    if (!(item->m_9c & 0x1000))
        flags |= 0x100000;
    return create(x, y, m_60, m_64, 0, flags, item, 0, 0);
}

// MATCH: golf_clean.exe 0x0049f9a0 ?slot30@M36OptionDialog@@UAEXHH@Z
void M36OptionDialog::slot30(int x, int y)
{
    g_83ab2c = (int)m_130;
    int col = x / (m_68 + m_48);
    int row = y / (m_6c + m_48);
    int id = idAt(col * m_5c + row);
    toggle(id);
    if (idAt(m_4) != id) {
        drawItem(m_4, bitTest(m_4), 0);
        m_4 = findId(id);
    }
    if (m_8)
        m_8(m_f0);
    drawItem(findId(id), flagIsSet(id), 1);
    invalidate();
}

// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a45b0 ?draw@M36PanelSub@@UAEXXZ
void M36PanelSub::draw()
{
    M36Win* parent = m_130;
    if (!parent)
        return;
    parent->blit(this, m_28, m_2c, 0, 0, width(), height());
    for (int i = 0; i < m_88; i++)
        drawItem(i);
}

// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x0049ff60 ??_GM36OptionDialog@@UAEPAXI@Z
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a4980 ??_GM36PanelSub@@UAEPAXI@Z
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a3050 ??_GM36PanelRow@@UAEPAXI@Z
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x00491900 ??_GM36View@@UAEPAXI@Z

// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a2ff0 ?cycleSelection@M36PanelRow@@UAEXXZ
void M36PanelRow::cycleSelection()
{
    if (m_140 && m_138 && m_13c->m_4) {
        m_13c->m_4->slot74();
        return;
    }
    M36Win::cycleSelection();
}

// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a1130 ?slot6@M36View@@UAEHXZ
int M36View::slot6()
{
    switch (m_kind) {
    case 2: return M36ListBox::slot6();
    }
    return 0;
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a1170 ?slot7@M36View@@UAEHH@Z
int M36View::slot7(int a)
{
    switch (m_kind) {
    case 2: return M36ListBox::slot7(a);
    }
    return a;
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0d30 ?draw@M36View@@UAEXXZ
void M36View::draw()
{
    switch (m_kind) {
    case 16: M36ListPanel::draw(); break;
    case 2: M36ListBox::draw(); break;
    case 1: M36OptionDialog::draw(); break;
    case 8: M36PanelSub::draw(); break;
    case 4: M36PanelRow::draw(); break;
    }
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a13c0 ?scrollBy@M36View@@UAEXHHH@Z
void M36View::scrollBy(int a, int b, int c)
{
    switch (m_kind) {
    case 2: M36ListBox::scrollBy(a, b, c); break;
    }
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0ab0 ?slot27@M36View@@UAEXHH@Z
void M36View::slot27(int a, int b)
{
    switch (m_kind) {
    case 16: M36ListPanel::slot27(a, b); break;
    case 2: M36ListBox::slot27(a, b); break;
    case 1: M36OptionDialog::slot27(a, b); break;
    case 8: M36PanelSub::slot27(a, b); break;
    }
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0a10 ?slot28@M36View@@UAEXHH@Z
void M36View::slot28(int a, int b)
{
    switch (m_kind) {
    case 16: M36ListPanel::slot28(a, b); break;
    case 2: M36ListBox::slot28(a, b); break;
    case 1: M36OptionDialog::slot28(a, b); break;
    case 8: M36PanelSub::slot28(a, b); break;
    case 4: return;
    }
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0db0 ?slot29@M36View@@UAEXHH@Z
void M36View::slot29(int a, int b)
{
    switch (m_kind) {
    case 16: case 1: case 4: return;
    case 2: M36ListBox::slot29(a, b); break;
    case 8: M36PanelSub::slot29(a, b); break;
    }
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0b50 ?slot30@M36View@@UAEXHH@Z
void M36View::slot30(int a, int b)
{
    switch (m_kind) {
    case 16: M36ListPanel::slot30(a, b); break;
    case 2: M36ListBox::slot30(a, b); break;
    case 1: M36OptionDialog::slot30(a, b); break;
    case 8: M36PanelSub::slot30(a, b); break;
    case 4: return;
    }
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0e60 ?slot31@M36View@@UAEXHH@Z
void M36View::slot31(int a, int b)
{
    switch (m_kind) {
    case 4: M36OptionDialog::slot31(a, b); break;
    case 8: M36PanelSub::slot31(a, b); break;
    }
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0eb0 ?slot32@M36View@@UAEXHH@Z
void M36View::slot32(int a, int b)
{
    if (m_kind == 8)
        M36PanelSub::slot32(a, b);
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0e20 ?slot33@M36View@@UAEXHH@Z
void M36View::slot33(int a, int b)
{
    if (m_kind == 8)
        M36PanelSub::slot33(a, b);
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0e90 ?slot34@M36View@@UAEXHH@Z
void M36View::slot34(int a, int b)
{
    if (m_kind == 8)
        M36PanelSub::slot34(a, b);
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0bf0 ?slot36@M36View@@UAEHH@Z
int M36View::slot36(int a)
{
    switch (m_kind) {
    case 16: return M36ListPanel::slot36(a);
    case 2: return M36ListBox::slot36(a);
    case 1: return M36OptionDialog::slot36(a);
    case 8: return M36PanelSub::slot36(a);
    case 4: return 0;
    }
    return 0;
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0c90 ?selectChanged@M36View@@UAEXHH@Z
void M36View::selectChanged(int a, int b)
{
    switch (m_kind) {
    case 16: M36ListPanel::selectChanged(a, b); break;
    case 2: M36ListBox::selectChanged(a, b); break;
    case 1: M36OptionDialog::selectChanged(a, b); break;
    case 8: M36PanelSub::selectChanged(a, b); break;
    }
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0e40 ?slot39@M36View@@UAEXHH@Z
void M36View::slot39(int a, int b)
{
    if (m_kind == 8)
        M36PanelSub::slot39(a, b);
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0ed0 ?onScroll@M36View@@UAEXHH@Z
void M36View::onScroll(int a, int b)
{
    switch (m_kind) {
    case 2: M36ListBox::onScroll(a, b); break;
    }
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a0ff0 ?slot60@M36View@@UAEXH@Z
void M36View::slot60(int a)
{
    switch (m_kind) {
    case 16: M36ListPanel::slot60(a); break;
    case 2: M36ListBox::slot60(a); break;
    case 1: M36OptionDialog::slot60(a); break;
    case 8: M36PanelSub::slot60(a); break;
    case 4: M36PanelRow::slot60(a); break;
    }
}
// MATCHED-IN re/match/m36d_view.cpp: golf_clean.exe 0x004a1090 ?cycleSelection@M36View@@UAEXXZ
void M36View::cycleSelection()
{
    switch (m_kind) {
    case 16: M36ListPanel::cycleSelection(); break;
    case 2: M36ListBox::cycleSelection(); break;
    case 1: M36OptionDialog::cycleSelection(); break;
    case 8: M36PanelSub::cycleSelection(); break;
    case 4: M36PanelRow::cycleSelection(); break;
    }
}
// Ghidra's extent of 0x004a0ef0 stops at the jump (0x004a0ef0-0x004a0f0f + the default 0x004a0fb6): the case blocks
// run to 0x004a0fbe, so it needs a re/match/boundaries.tsv row (golf_clean.exe 004a0ef0 206) to be compared. With that
// extent it is 100% (64/64, tables equal; checked in memory, log/c3/m36d_notes.txt). The line below is tagged
// MATCH-PENDING so that match.py writes no RED CSV before the row exists: rename it to MATCH after adding the row.
// MATCH-PENDING: golf_clean.exe 0x004a0ef0 ?build@M36View@@UAEHPAUM36Item@@HHH@Z
int M36View::build(M36Item* a, int b, int c, int d)
{
    switch (m_kind) {
    case 16: return M36ListPanel::build(a, b, c, d);
    case 2: return M36ListBox::build(a, b, c, d);
    case 1: return M36OptionDialog::build(a, b, c, d);
    case 8: return M36PanelSub::build(a, b, c, d);
    case 4: return M36PanelRow::build(a, b, c, d);
    }
    return 7;
}
