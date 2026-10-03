// golf_clean.exe 0x00401000 and 0x004762d0 (release). WIP: clearRec74 81-94% (the original computes the
// second memset address before `mov ecx, 9`; tried member pointer); Sel476::select 82% (store order differs,
// same symptom as golf_select.cpp). Not in the 100% suite.
// FLAGS golf_clean.exe: /O2
#include <string.h>

struct Rec74 { int a[8]; char pad[0x50 - 0x20]; int b[9]; };      // 0x74 bytes
extern Rec74 g_rec74[];                  // 0x4e6d20, stride 0x74

// MATCH: golf_clean.exe 0x00401000 ?clearRec74@@YAXH@Z
void clearRec74(int i)
{
    memset(g_rec74[i].a, 0xff, sizeof(g_rec74[i].a));
    memset(g_rec74[i].b, 0xff, sizeof(g_rec74[i].b));
}

struct Handle4b { int m0; int m4; };
class Sel476 {
public:
    int select(Handle4b* h, int a, int b, int c);   // 0x4762d0
    char pad[0x5c];
    Handle4b* m_5c; int m_60, m_64, m_68;
};

// MATCH: golf_clean.exe 0x004762d0 ?select@Sel476@@QAEHPAUHandle4b@@HHH@Z
int Sel476::select(Handle4b* h, int a, int b, int c)
{
    if (!h)
        return 3;
    if (h->m4) {
        m_5c = h;
        m_60 = a;
        m_64 = b;
        m_68 = c;
    }
    return 0;
}

