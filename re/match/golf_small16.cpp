// Small golf_clean.exe functions, batch 16 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <string.h>

struct Target482 { int find(void* who, int flags); };   // 0x482420
struct Link482 { int m_0; Target482* m_4; int resolve(); };
// Returns 0 when the link has no target, otherwise the target's find(this, 0).
// MATCH: golf_clean.exe 0x00482e20 ?resolve@Link482@@QAEHXZ
int Link482::resolve()
{
    if (m_4 == 0)
        return 0;
    return m_4->find(this, 0);
}

struct Mgr83b { void set(int a, int b); };   // 0x487fb0
extern Mgr83b g_mgr83b;                      // 0x83b000
// MATCH: golf_clean.exe 0x00487e70 ?mgrSet@@YAXHH@Z
void mgrSet(int a, int b)
{
    g_mgr83b.set(a, b);
}

void step492570(int a);                      // 0x492570
void step4924e0(int a);                      // 0x4924e0
// MATCH: golf_clean.exe 0x004925b0 ?runBoth@@YAXH@Z
void runBoth(int a)
{
    step492570(a);
    step4924e0(a);
}

// Cuts the string at its last newline.
// MATCH: golf_clean.exe 0x004925d0 ?cutLastNewline@@YAXPAD@Z
void cutLastNewline(char* s)
{
    char* p = strrchr(s, '\n');
    if (p)
        *p = 0;
}

struct Obj820 { void refresh(); };           // 0x4a4f34
extern Obj820* g_obj820;                     // 0x820ed0
void after449540();                          // 0x449540
extern char g_flag820f2a;                    // 0x820f2a
// MATCH: golf_clean.exe 0x00449520 ?refresh820@@YAXXZ
void refresh820()
{
    g_obj820->refresh();
    after449540();
    g_flag820f2a = 1;
}

void pre49c8e0();                            // 0x49c8e0
void pre497b20();                            // 0x497b20
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
    virtual void v74();
    virtual void v75();
    virtual void v76();
    virtual void redraw();                   // vtable +0x134
};
extern Screen83a* g_screen83a;               // 0x83ad50
// MATCH: golf_clean.exe 0x00483c70 ?redrawAll@@YAXXZ
void redrawAll()
{
    pre49c8e0();
    pre497b20();
    g_screen83a->redraw();
}
