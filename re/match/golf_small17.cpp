// Small golf_clean.exe functions, batch 17 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <windows.h>

struct Gfx840 { void flushA(); void flushB(); };   // 0x49aa70, 0x49ab40
extern Gfx840* g_gfx840;                     // 0x8400b8
extern int g_gfx840ready;                    // 0x8400b0
// MATCH: golf_clean.exe 0x00497b00 ?gfxFlushA@@YAXXZ
void gfxFlushA()
{
    if (g_gfx840 && g_gfx840ready)
        g_gfx840->flushA();
}

// MATCH: golf_clean.exe 0x00497b20 ?gfxFlushB@@YAXXZ
void gfxFlushB()
{
    if (g_gfx840 && g_gfx840ready)
        g_gfx840->flushB();
}

struct Panel47e;
struct Owner47e { void notify(Panel47e* p); };   // 0x47e700
void notifyDefault(Panel47e* p);             // 0x47e580
// Notifies the owner at +0x130, or the default handler when there is none.
struct Panel47e { char pad[0x130]; Owner47e* m_owner; void notify(); };
// MATCH: golf_clean.exe 0x0047e120 ?notify@Panel47e@@QAEXXZ
void Panel47e::notify()
{
    if (m_owner)
        m_owner->notify(this);
    else
        notifyDefault(this);
}

void detach47f8e0(int a, int b);             // 0x47f8e0
void free47cdb0(int a);                      // 0x47cdb0
// MATCH: golf_clean.exe 0x00495770 ?release495@@YGXH@Z
void __stdcall release495(int a)
{
    detach47f8e0(a, 0);
    free47cdb0(a);
}

struct Slot80d { char pad[0x6c]; void start(int ms); };   // start: 0x484750
extern Slot80d g_slots80d[];                 // 0x80d840, stride 0x6c
// MATCH: golf_clean.exe 0x00448200 ?startSlot@@YAXH@Z
void startSlot(int i)
{
    g_slots80d[i].start(1000);
}

void pre49c8e0();                            // 0x49c8e0
struct Screen83a {
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual void v4();
    virtual void v5();
    virtual void v6();
    virtual void v7();
    virtual void v8();
    virtual void v9();
    virtual void v10();
    virtual void v11();
    virtual void v12();
    virtual void v13();
    virtual void v14();
    virtual void v15();
    virtual void v16();
    virtual void v17();
    virtual void v18();
    virtual void v19();
    virtual void v20();
    virtual void v21();
    virtual void v22();
    virtual void v23();
    virtual void v24();
    virtual void v25();
    virtual void v26();
    virtual void v27();
    virtual void v28();
    virtual void v29();
    virtual void v30();
    virtual void v31();
    virtual void v32();
    virtual void v33();
    virtual void v34();
    virtual void v35();
    virtual void v36();
    virtual void v37();
    virtual void v38();
    virtual void v39();
    virtual void v40();
    virtual void v41();
    virtual void v42();
    virtual void v43();
    virtual void v44();
    virtual void v45();
    virtual void v46();
    virtual void v47();
    virtual void v48();
    virtual void v49();
    virtual void v50();
    virtual void v51();
    virtual void v52();
    virtual void v53();
    virtual void v54();
    virtual void v55();
    virtual void v56();
    virtual void v57();
    virtual void v58();
    virtual void v59();
    virtual void v60();
    virtual void v61();
    virtual void v62();
    virtual void v63();
    virtual void v64();
    virtual void v65();
    virtual void v66();
    virtual void v67();
    virtual void v68();
    virtual void v69();
    virtual void v70();
    virtual void v71();
    virtual void v72();
    virtual void v73();
    virtual void show();                     // vtable +0x128
    virtual void hide();                     // vtable +0x12c
};
extern Screen83a* g_screen83a;               // 0x83ad50
// MATCH: golf_clean.exe 0x00483bb0 ?showScreen@@YAXXZ
void showScreen()
{
    pre49c8e0();
    gfxFlushA();
    gfxFlushB();
    g_screen83a->show();
}

// MATCH: golf_clean.exe 0x00483c10 ?hideScreen@@YAXXZ
void hideScreen()
{
    pre49c8e0();
    gfxFlushB();
    gfxFlushA();
    g_screen83a->hide();
}

extern int g_cursorW, g_cursorH;             // 0x83b5f8, 0x83b5fc
// Caches the cursor size (SM_CXCURSOR 13, SM_CYCURSOR 14).
// MATCH: golf_clean.exe 0x004884e0 ?cacheCursorSize@@YAHXZ
int cacheCursorSize()
{
    g_cursorW = GetSystemMetrics(SM_CXCURSOR);
    g_cursorH = GetSystemMetrics(SM_CYCURSOR);
    return 0;
}
