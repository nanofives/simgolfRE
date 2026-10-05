// FLAGS golf_clean.exe: /O2 /GX
#include <windows.h>
#include <mmsystem.h>
#define V8(p) virtual void p##0(); virtual void p##1(); virtual void p##2(); virtual void p##3(); virtual void p##4(); virtual void p##5(); virtual void p##6(); virtual void p##7();
struct R0Screen { V8(a) virtual HWND hwnd(); };
extern R0Screen* g_83ad50;
void __cdecl r0_f483da0();
struct R0C486ec0 {
    char pad[8];
    unsigned int m_8;
    UINT m_c;
    char pad1[0x20 - 0x10];
    unsigned int m_20;
    void stop();
};
// MATCH: golf_clean.exe 0x00486ec0 ?stop@R0C486ec0@@QAEXXZ
void R0C486ec0::stop()
{
    int f = ~m_8 & 1;
    if (m_c) {
        if (m_20 < 50)
            timeKillEvent(m_c);
        else if (g_83ad50)
            KillTimer(g_83ad50->hwnd(), m_c);
        m_c = 0;
    }
    if (f)
        r0_f483da0();
}
