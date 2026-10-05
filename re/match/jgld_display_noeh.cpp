// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll display creation/teardown: this translation unit has no C++ exception frames (the `new Display`
// has no EH cleanup), so it is compiled without /GX. Names are chosen here.
#include <windows.h>
extern CRITICAL_SECTION g_cs;    // 0x101286b0
struct DisplayBase { DisplayBase(); virtual ~DisplayBase(); };   // 0x10065630
struct DisplayMember { DisplayMember(); char pad[0x12c - 4]; }; // +4, ctor 0x10069270
class Display : public DisplayBase {
public:
    Display();
    virtual ~Display();
    DisplayMember m_4;
    int m_12c, m_130, m_134, m_138, m_13c, m_140, m_144, m_148;
};
extern Display* g_display;       // 0x10128420
extern Display* g_main;          // 0x1012870c
struct Tracked { static void deleteAll(); };
class Lib {
public:
    void destroyDisplay();
};
// MATCH: jgld.dll 0x10065390 ?createDisplay@@YAPAVDisplay@@XZ
Display* createDisplay()
{
    Display* d = new Display;
    g_main = d;
    g_display = g_main;
    return g_main;
}
// MATCH: jgld.dll 0x10065420 ?destroyDisplay@Lib@@QAEXXZ
void Lib::destroyDisplay()
{
    Tracked::deleteAll();
    delete g_main;
}
