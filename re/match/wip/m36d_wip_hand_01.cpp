// FLAGS golf_clean.exe: /O2
// m36d near misses (round 36): copy of re/match/m36d_hand_01.cpp whose MATCH lines are only the functions below
// 100%. Kept in wip/ so the 100% test and the evidence run never see them; what was tried is in
// log/c3/m36d_notes.txt.
// m36d: small golf_clean.exe functions written by hand (release /O2). Class and member names are placeholders;
// every offset, constant and callee is the original's (log/c3/m36d_notes.txt). Callee conventions from their `ret N`.
#include <string.h>
#include <stdio.h>
#pragma vtordisp(off)
#define V4(a, b, c, d) virtual void v##a(); virtual void v##b(); virtual void v##c(); virtual void v##d();
#define V16(x) V4(x##0, x##1, x##2, x##3) V4(x##4, x##5, x##6, x##7) V4(x##8, x##9, x##a, x##b) V4(x##c, x##d, x##e, x##f)

// ---- 0x00486b20 EditBox::cycleSelectionIfOwned: Container::cycleSelection 0x0047af60 when +0x130 is set
struct H1Container {
    void cycleSelection();      // 0x0047af60
    char pad[0x130];
    int m_130;
    void cycleSelectionIfOwned();
};
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00486b20 ?cycleSelectionIfOwned@H1Container@@QAEXXZ
void H1Container::cycleSelectionIfOwned()
{
    if (m_130)
        cycleSelection();
}

// ---- timer callbacks: (id, object) -> object->onTimer(id)
struct H1Button { void onTimer(int); /* 0x00488fd0 */ char pad[0x5f8]; int m_5f8; char pad2[0x614 - 0x5fc]; int m_614; char pad3[0x620 - 0x618]; int m_620;
    void onTipTimer(int, int); void detach(); void windowDetach(); /* 0x0047b820 */ };
struct H1HotList { void showTip(int); /* 0x00492bd0 */ };
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00488fb0 ?buttonTimerCallback@@YAXHPAUH1Button@@@Z
void buttonTimerCallback(int id, H1Button* b)
{
    if (b)
        b->onTimer(id);
}
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00492bb0 ?hotListTipTimerCallback@@YAXHPAUH1HotList@@@Z
void hotListTipTimerCallback(int id, H1HotList* h)
{
    if (h)
        h->showTip(id);
}
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00488e90 ?onTipTimer@H1Button@@QAEXHH@Z
void H1Button::onTipTimer(int, int)
{
    if (m_5f8)
        onTimer(0);
}
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00489120 ?detach@H1Button@@QAEXXZ
void H1Button::detach()
{
    if (!m_620)
        m_614 = 0;
    windowDetach();
}

// ---- 0x004840e0: forwards to the sound library entry at 0x0083af6c when the library is up (0x0083afc4)
extern int g_83afc4;
extern int (*g_83af6c)(int, int, int);
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x004840e0 ?createSound4840e0@@YAHHHH@Z
int createSound4840e0(int a, int b, int c)
{
    if (!g_83afc4)
        return 1;
    return g_83af6c(a, b, c);
}

// ---- 0x00487210 StrList constructor
struct H1StrBuf { char* m_p; H1StrBuf() { m_p = 0; } };
struct H1StrList {
    virtual void v0();
    H1StrBuf m_4;
    int m_8, m_c, m_10, m_14, m_18, m_1c;
    H1StrList();
};
// MATCH: golf_clean.exe 0x00487210 ??0H1StrList@@QAE@XZ
H1StrList::H1StrList()
{
    m_8 = 0x7f;
    m_c = 0;
    m_18 = 0;
    m_1c = 0;
    m_10 = 0;
    m_14 = 0;
}

// ---- 0x004627d0 resetSpriteQueue (the loop stores -1 into the same dword 0x400 times)
extern int g_838200, g_834174, g_8371fc, g_82c160;
extern volatile int g_8371f4;
// MATCH: golf_clean.exe 0x004627d0 ?resetSpriteQueue@@YAXXZ
void resetSpriteQueue()
{
    g_838200 = 0;
    g_834174 = 0;
    g_8371fc = 0;
    g_82c160 = -1;
    for (int i = 0x400; i; ) {
        i--;
        g_8371f4 = -1;
    }
}

// ---- 0x00482b60 Flic base constructor: Surface member at +8 (ctor 0x00473ab0)
struct H1Surface { H1Surface(); ~H1Surface(); char pad[0x2c]; };
struct H1Flic {
    virtual void v0();
    int m_4;
    H1Surface m_8;
    int m_34, m_38, m_3c, m_40;
    H1Flic();
};
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00482b60 ??0H1Flic@@QAE@XZ
H1Flic::H1Flic()
{
    m_4 = 0;
    m_38 = 0;
    m_3c = 0;
    m_34 = 0;
    m_40 = -1;
}

// ---- 0x00485260 Snd constructor
struct H1Snd {
    virtual void v0();
    int m_4, m_8;
    int m_c[9];
    int m_30;
    H1Snd();
};
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00485260 ??0H1Snd@@QAE@XZ
H1Snd::H1Snd()
{
    m_4 = 0x7f;
    m_8 = 0;
    memset(m_c, 0, sizeof(m_c));
    m_30 = 0;
}

// ---- 0x00487060 BinkPlayer::close: the two binkw32 imports at 0x004ba260 / 0x004ba224
extern "C" __declspec(dllimport) void __stdcall BinkClose(void*);
extern "C" __declspec(dllimport) void __stdcall BinkBufferClose(void*);
struct H1Bink { int m_0; void* m_4; void* m_8; void close(); };
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00487060 ?close@H1Bink@@QAEXXZ
void H1Bink::close()
{
    if (m_4)
        BinkClose(m_4);
    if (m_8)
        BinkBufferClose(m_8);
    m_4 = 0;
    m_8 = 0;
}

// ---- 0x004889f0 / 0x00490cf0: default font and colours for buttons / message boxes
struct H1Font { int m_0; int m_4; };
extern H1Font* g_83b60c; extern int g_83b610, g_83b614;
extern H1Font* g_83b9c4; extern int g_83b9c8, g_83b9cc;
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x004889f0 ?setButtonDefaults@@YAHPAUH1Font@@HH@Z
int setButtonDefaults(H1Font* f, int a, int b)
{
    if (!f)
        return 3;
    if (f->m_4)
        g_83b60c = f;
    g_83b610 = a;
    g_83b614 = b;
    return 0;
}
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00490cf0 ?msgBoxSetButtonB@@YAHPAUH1Font@@HH@Z
int msgBoxSetButtonB(H1Font* f, int a, int b)
{
    if (!f)
        return 3;
    if (f->m_4)
        g_83b9c4 = f;
    g_83b9c8 = a;
    g_83b9cc = b;
    return 0;
}

// ---- 0x004832e0 startColorCycle: 16-byte entries {.., key at +8, Timer* at +0xc}
struct H1Timer { void startMmPeriodic(); /* 0x00486dc0 */ };
struct H1CycleEntry { int m_0, m_4, m_8; H1Timer* m_c; };
struct H1CycleTable {
    H1CycleEntry m_e[1];
    int find(int);              // 0x004833f0
    int start(int);
};
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x004832e0 ?start@H1CycleTable@@QAEHH@Z
int H1CycleTable::start(int key)
{
    H1CycleEntry* e = &m_e[find(key)];
    if (e->m_8 != key)
        return 0xb;
    e->m_c->startMmPeriodic();
    return 0;
}

// ---- 0x004873f0 / 0x004879b0 / 0x00487b90: open hooks through sound-library entries (filled by GetProcAddress)
extern int (*g_83af7c)(void**, int);
extern int (*g_83af84)(void**, int);
extern int (*g_83af8c)(void**, int);
struct H1Hook { char pad[0x14]; void* m_14; int openA(int); int openB(int); int openC(int); };
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x004873f0 ?openA@H1Hook@@QAEHH@Z
int H1Hook::openA(int a)
{
    if (!m_14) {
        if (g_83af7c)
            return g_83af7c(&m_14, a);
        return 0x14;
    }
    return 0xc;
}
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x004879b0 ?openB@H1Hook@@QAEHH@Z
int H1Hook::openB(int a)
{
    if (!m_14) {
        if (g_83af84)
            return g_83af84(&m_14, a);
        return 0x14;
    }
    return 0xc;
}
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00487b90 ?openC@H1Hook@@QAEHH@Z
int H1Hook::openC(int a)
{
    if (!m_14) {
        if (g_83af8c)
            return g_83af8c(&m_14, a);
        return 0x14;
    }
    return 0xc;
}

// ---- 0x00490bf0 MsgBox::initDefaults
extern int g_83ad44, g_83b9b4, g_83b99c, g_83b9a0;
extern char s_4c8590[], s_4e3db0[];
void msgBoxSetLabelB(const char*);  // 0x0048de90
void msgBoxSetLabelA(const char*);  // 0x0048de00
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00490bf0 ?msgBoxInitDefaults@@YAHXZ
int msgBoxInitDefaults()
{
    g_83b9b4 = g_83ad44;
    *(int*)&g_83b9c4 = g_83ad44;
    msgBoxSetLabelB(s_4c8590);
    msgBoxSetLabelA(s_4e3db0);
    g_83b99c = 0;
    g_83b9a0 = 0;
    return 0;
}

// ---- 0x00486d20 Timer::init
struct H1TimerInit { int m_0, m_4, m_8, m_c, m_10, m_14, m_18, m_1c, m_20, m_24, m_28; void reset(); /* 0x00486f10 */
    void init(int, int, int, int, int); };
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00486d20 ?init@H1TimerInit@@QAEXHHHHH@Z
void H1TimerInit::init(int a, int b, int c, int d, int e)
{
    reset();
    m_14 = a;
    m_18 = c;
    m_10 = 0;
    m_1c = b;
    m_20 = d;
    m_28 = e;
}

// ---- 0x0047cfd0: a local Image (ctor 0x00474ae0, dtor 0x00474c40), four globals cleared, emptyFunc_4a0890
struct H1Image { H1Image(); ~H1Image(); char pad[0x2b8]; };
extern int g_83a4e8, g_83a4ec, g_83a4f0, g_83a4f4;
void emptyFunc_4a0890();
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x0047cfd0 ?resetViewState@@YAHXZ
int resetViewState()
{
    H1Image img;
    g_83a4e8 = 0;
    g_83a4ec = 0;
    g_83a4f0 = 0;
    g_83a4f4 = 0;
    emptyFunc_4a0890();
    return 0;
}

// ---- EditBox caret drag: owner globals 0x0083ab30 / 0x0083ab34, caret at +0x5a4, refresh = window slot 72
struct H1EditBox {
    V16(0) V16(1) V16(2) V16(3) V4(40, 41, 42, 43) V4(44, 45, 46, 47)
    virtual void refresh();
    char pad[0x5a4 - 4];
    int m_5a4;
    int pixelToCharPos(int, int, int, int);     // 0x004863e0
    void beginCaretDrag(int, int);
    void updateCaretDrag(int, int);
    void endCaretDrag(int, int);
};
extern H1EditBox* g_83ab30;
extern H1EditBox* g_83ab34;
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00486b90 ?beginCaretDrag@H1EditBox@@QAEXHH@Z
void H1EditBox::beginCaretDrag(int x, int y)
{
    if (!g_83ab30) {
        g_83ab30 = this;
        g_83ab34 = 0;
    }
    m_5a4 = pixelToCharPos(x, y, 0, 1);
    refresh();
}
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00486c40 ?updateCaretDrag@H1EditBox@@QAEXHH@Z
void H1EditBox::updateCaretDrag(int x, int y)
{
    if (g_83ab30 == this || (g_83ab34 == this && g_83ab30 && g_83ab34)) {
        m_5a4 = pixelToCharPos(x, y, 0, 1);
        refresh();
    }
}
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00486be0 ?endCaretDrag@H1EditBox@@QAEXHH@Z
void H1EditBox::endCaretDrag(int x, int y)
{
    if (g_83ab30 == this) {
        g_83ab30 = 0;
        v04();
    }
    if (g_83ab34 == this)
        g_83ab34 = 0;
    m_5a4 = pixelToCharPos(x, y, 0, 1);
    refresh();
}

// ---- 0x00473c60 Surface_blitTakeDirty: blit, then take the source's dirty word (+0xac)
struct H1BlitSrc { char pad[0xac]; int m_ac; };
struct H1BlitDst {
    int m_0, m_4, m_8, m_c;
    int blit(H1BlitSrc*, int, int, int, int, int, int);     // 0x00473bf0
    int blitTakeDirty(H1BlitSrc*, int, int, int, int, int, int);
};
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00473c60 ?blitTakeDirty@H1BlitDst@@QAEHPAUH1BlitSrc@@HHHHHH@Z
int H1BlitDst::blitTakeDirty(H1BlitSrc* s, int a, int b, int c, int d, int e, int f)
{
    int r = blit(s, a, b, c, d, e, f);
    m_c = s->m_ac;
    s->m_ac = 0;
    return r;
}

// ---- 0x0047b9a0 Window::passLocalRect: the rect at +0x1ac moved to the origin, to return0Pop4_487b00
struct H1Rect {
    int l, t, r, b;
    void offset(int dx, int dy) { l += dx; r += dx; t += dy; b += dy; }
};
struct H1Win1ac {
    char pad[0x1ac];
    H1Rect m_1ac;
    int return0Pop4(H1Rect*);   // 0x00487b00
    void passLocalRect();
};
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x0047b9a0 ?passLocalRect@H1Win1ac@@QAEXXZ
void H1Win1ac::passLocalRect()
{
    H1Rect r = m_1ac;
    r.offset(-r.l, -r.t);
    return0Pop4(&r);
}

// ---- 0x00476700 / 0x00476750: draw with a temporary font (+0x5c..+0x64 saved and restored)
struct H1TextFont { int m_0, m_4; };
struct H1FontState { H1TextFont* m_font; int m_4, m_8; };
struct H1Text {
    char pad[0x5c];
    H1FontState m_5c;
    void drawStringCentered(int, int, const char*, int);    // 0x00477fc0
    void drawStringRightAt(int, int, const char*, int);     // 0x00478140
    void drawStringAt(H1TextFont*, int, int, const char*, int);
    void drawStringAt2(H1TextFont*, int, int, const char*, int);
};
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00476700 ?drawStringAt@H1Text@@QAEXPAUH1TextFont@@HHPBDH@Z
void H1Text::drawStringAt(H1TextFont* f, int x, int y, const char* s, int c)
{
    H1FontState saved = m_5c;
    if (f && f->m_4) {
        m_5c.m_font = f;
        m_5c.m_4 = 0;
        m_5c.m_8 = 0;
    }
    drawStringCentered(x, y, s, c);
    m_5c = saved;
}
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00476750 ?drawStringAt2@H1Text@@QAEXPAUH1TextFont@@HHPBDH@Z
void H1Text::drawStringAt2(H1TextFont* f, int x, int y, const char* s, int c)
{
    H1FontState saved = m_5c;
    if (f && f->m_4) {
        m_5c.m_font = f;
        m_5c.m_4 = 0;
        m_5c.m_8 = 0;
    }
    drawStringRightAt(x, y, s, c);
    m_5c = saved;
}

// ---- 0x004657e0 / 0x00465840 root-window press handlers: scaled position and press state
int scaleX(int);    // 0x00404970
extern int g_822d58, g_822d5c, g_822d60, g_822d64, g_822d68, g_822d74, g_822d78;
struct H1RootWin { void onPress1b(int, int); void onPress2b(int, int); };   // vtable 0x004ba2fc slots
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x004657e0 ?onPress1b@H1RootWin@@QAEXHH@Z
void H1RootWin::onPress1b(int x, int y)
{
    g_822d58 = x * 2 / scaleX(2);
    g_822d5c = y * 2 / scaleX(2);
    g_822d60 = x;
    g_822d64 = y;
    g_822d68 = 1;
    g_822d78 = 1;
    g_822d74 = 1;
}
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00465840 ?onPress2b@H1RootWin@@QAEXHH@Z
void H1RootWin::onPress2b(int x, int y)
{
    g_822d58 = x * 2 / scaleX(2);
    g_822d5c = y * 2 / scaleX(2);
    g_822d60 = x;
    g_822d64 = y;
    g_822d68 = 2;
    g_822d78 = 1;
    g_822d74 = 1;
}

// ---- 0x0046ddd0 startNewCourse
extern int g_822c88, g_571fd4, g_834170, g_59e7b8;
int loadGameBrowserScreen(int, int);    // 0x0043b610
int pickAProScreen();                   // 0x0043a8c0
void loadGolfPro();                     // 0x0046c970
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x0046ddd0 ?startNewCourse@@YAHXZ
int startNewCourse()
{
    int saved = g_822c88;
    if (!loadGameBrowserScreen(1, 1))
        return 0;
    g_822c88 = saved;
    g_571fd4 = 1000;
    g_834170 = 0x2c00;
    g_59e7b8 = 0x4000000;
    if (!pickAProScreen()) {
        g_59e7b8 = 0;
        return 0;
    }
    loadGolfPro();
    g_571fd4 = 1000;
    return 1;
}

// ---- 0x00488230 Reader::readLine: fgets into the 0x200-byte line buffer, strip and trim
struct H1File { char pad[0xc]; int m_c; };
void stripNewline(char*);   // 0x004925d0
void trimSpaces(char*);     // 0x004925b0
struct H1Reader { char pad[0x154]; char* m_154; H1File* m_158; char* m_15c; char* readLine(); };
// MATCHED-IN re/match/m36d_hand_01.cpp: golf_clean.exe 0x00488230 ?readLine@H1Reader@@QAEPADXZ
char* H1Reader::readLine()
{
    if (m_158->m_c & 0x10) {
        *m_15c = 0;
        return 0;
    }
    if (!fgets(m_15c, 0x1ff, (FILE*)m_158)) {
        *m_15c = 0;
        return m_15c;
    }
    stripNewline(m_15c);
    trimSpaces(m_15c);
    m_154 = m_15c;
    return m_15c;
}
