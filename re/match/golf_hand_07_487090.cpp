// FLAGS golf_clean.exe: /O2 /GX
#include <windows.h>
#include "golf_hand_07_hdr.h"
extern "C" {
__declspec(dllimport) int __stdcall BinkOpenDirectSound(unsigned long);
__declspec(dllimport) int __stdcall BinkSetSoundSystem(void*, unsigned long);
__declspec(dllimport) void __stdcall BinkSetIOSize(unsigned long);
__declspec(dllimport) void* __stdcall BinkOpen(const char*, unsigned long);
__declspec(dllimport) void* __stdcall BinkBufferOpen(void*, unsigned long, unsigned long, unsigned long);
__declspec(dllimport) int __stdcall BinkBufferSetOffset(void*, int, int);
__declspec(dllimport) int __stdcall BinkWait(void*);
}
struct Bink487090 { unsigned width, height; int pad[5]; int readError; };
extern char g_83ab9c;
void pump483c90();   // 0x483c90
void pump483cf0();   // 0x483cf0
struct C487090 {
    int m_0;
    Bink487090* m_bink;
    void* m_buf;
    void* m_hwnd;
    int m_x, m_y;
    bool frame();   // 0x487180
    int play(const char* name, int x, int y, unsigned long snd);
};
// MATCH: golf_clean.exe 0x00487090 ?play@C487090@@QAEHPBDHHK@Z
int C487090::play(const char* name, int x, int y, unsigned long snd)
{
    if (!m_hwnd)
        return 7;
    if (snd)
        BinkSetSoundSystem(BinkOpenDirectSound, snd);
    BinkSetIOSize(0x9c4000);
    m_bink = (Bink487090*)BinkOpen(name, 0x1000000);
    if (m_bink && !m_bink->readError) {
        m_buf = BinkBufferOpen(m_hwnd, m_bink->width, m_bink->height, 0);
        if (m_buf) {
            m_x = x;
            m_y = y;
            BinkBufferSetOffset(m_buf, x, y);
            g_83ab9c = 1;
            ShowCursor(FALSE);
            for (;;) {
                if (!BinkWait(m_bink)) {
                    if (!g_83ab9c || !frame())
                        break;
                } else {
                    pump483c90();
                    pump483cf0();
                }
            }
            ShowCursor(TRUE);
            return 0;
        }
    }
    return 1;
}
