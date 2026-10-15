// FLAGS golf_clean.exe: /O2 /GX
// m36d: golf_clean.exe functions whose original has an EH frame or class-level operator delete (release /O2 /GX).
// Names are placeholders; offsets, constants and callees are the original's (log/c3/m36d_notes.txt).
#include <stdlib.h>
#include <list>
#pragma vtordisp(off)

// ---- 0x004042f0: scalar deleting destructor of the class whose destructor is 0x00404280
struct HXObj404280 { HXObj404280(); virtual ~HXObj404280(); int m_4; };
HXObj404280::HXObj404280() {}
// MATCH: golf_clean.exe 0x004042f0 ??_GHXObj404280@@UAEPAXI@Z

// ---- 0x0044ae60 / 0x0044b060: constructors of global objects made of Surface members (ctor 0x00473ab0)
struct HXSurface { HXSurface(); ~HXSurface(); char pad[0x2c]; };
struct HXSurf2 { HXSurface m_0, m_2c; HXSurf2(); };
struct HXSurf6 { HXSurface m_0, m_2c, m_58, m_84, m_b0, m_dc; HXSurf6(); };
// MATCH: golf_clean.exe 0x0044ae60 ??0HXSurf2@@QAE@XZ
HXSurf2::HXSurf2() {}
// MATCH: golf_clean.exe 0x0044b060 ??0HXSurf6@@QAE@XZ
HXSurf6::HXSurf6() {}

// ---- 0x00483750: deleting destructor of a class with the heap-holder base (vtable 0x004ba278) as virtual base at
// +0x20; the holder's constructor takes the pending heap 0x00839650 and its destructor gives it back, and its operator
// delete frees only when no heap is pending (golf_hand_r1.cpp R1V482f60 / R1V489370 use the same holder).
extern int g_839650;
struct HXHeapHolder {
    HXHeapHolder() { m_4 = g_839650; g_839650 = 0; }
    virtual ~HXHeapHolder() { g_839650 = m_4; }
    void operator delete(void* p)
    {
        if (p) {
            if (!g_839650)
                free(p);
            g_839650 = 0;
        }
    }
    int m_4;
};
struct HXCache : virtual HXHeapHolder {
    HXCache();
    virtual ~HXCache();     // 0x004835f0
    char pad[0x20 - 4];
};
HXCache::HXCache() {}
// MATCH: golf_clean.exe 0x00483750 ??_GHXCache@@UAEPAXI@Z

// ---- 0x00449210: construct routine of the global std::list at 0x00820f18 (node 0xc bytes: next, prev, value)
std::list<int> g_00820f18;
// In this obj the global's routines are _$E2 construct, _$E3 destroy (std::list dtor inlined), _$E4 atexit
// registration, _$E5 initializer; the initializer stub 0x00449200 is matched in m36d_init.cpp.
// MATCH: golf_clean.exe 0x00449210 _$E2

// ---- 0x004799d0 storeUiEventTable
extern int g_83a4f8;
extern char g_4ba87c[];
// MATCH: golf_clean.exe 0x004799d0 ?storeUiEventTable@@YAXXZ
void storeUiEventTable()
{
    g_83a4f8 = (int)g_4ba87c;
}
