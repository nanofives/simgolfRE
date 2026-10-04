// Small golf_clean.exe functions, batch 18 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <stdlib.h>
#include <stdio.h>

extern void* g_buf840820;                    // 0x840820
// MATCH: golf_clean.exe 0x0049d460 ?freeBuf840820@@YAXXZ
void freeBuf840820()
{
    if (g_buf840820) {
        free(g_buf840820);
        g_buf840820 = 0;
    }
}

// Scalar deleting destructors (compiler generated from a virtual destructor; bodies 0x486ce0 and 0x492dc0).
struct Obj486 { virtual ~Obj486(); };
Obj486* newObj486() { return new Obj486; }
// MATCH: golf_clean.exe 0x00486cc0 ??_GObj486@@UAEPAXI@Z
struct Obj492 { virtual ~Obj492(); };
Obj492* newObj492() { return new Obj492; }
// MATCH: golf_clean.exe 0x00492da0 ??_GObj492@@UAEPAXI@Z

struct Entry483 { int a, b, key, c; };
struct Table483 { Entry483 e[5]; int find(int key); };
// Index of `key` among the 5 entries, or of the first free one (-1); 5 when neither is found.
// MATCH: golf_clean.exe 0x004833f0 ?find@Table483@@QAEHH@Z
int Table483::find(int key)
{
    int i;
    for (i = 0; i < 5; i++) {
        if (key == e[i].key || e[i].key == -1)
            break;
    }
    return i;
}

struct File487 { char pad[0x158]; FILE* m_file; void close(); };
// MATCH: golf_clean.exe 0x00487f80 ?close@File487@@QAEXXZ
void File487::close()
{
    if (m_file) {
        fclose(m_file);
        m_file = 0;
    }
}

extern int g_hook83afc0;                     // 0x83afc0
extern int (*g_fn83af74)(int, int);          // 0x83af74
// Returns 4 while the hook is not installed, otherwise calls it.
// MATCH: golf_clean.exe 0x00484090 ?callHook83a@@YAHHH@Z
int callHook83a(int a, int b)
{
    if (!g_hook83afc0)
        return 4;
    return g_fn83af74(a, b);
}

extern int g_keep83d348;                     // 0x83d348
struct Buf492 { char pad[0x1c]; void* m_data; void release(); };
// MATCH: golf_clean.exe 0x00492660 ?release@Buf492@@QAEXXZ
void Buf492::release()
{
    if (g_keep83d348 == 0 && m_data) {
        free(m_data);
        m_data = 0;
    }
}

// Visible when flag bit 0 of +0xa0 is set here and on every parent (+0x130).
struct Win480 { char pad[0xa0]; int m_flags; char pad2[0x8c]; Win480* m_parent; int visible(); };
// MATCH: golf_clean.exe 0x004801f0 ?visible@Win480@@QAEHXZ
int Win480::visible()
{
    if (!(m_flags & 1))
        return 0;
    if (m_parent && !m_parent->visible())
        return 0;
    return 1;
}

struct Target482 { int send(int a, int b, void* from, int c); };   // 0x482490
struct Link482b { int m_0; Target482* m_4; int send(int a, int b, int c); };
// MATCH: golf_clean.exe 0x00482e40 ?send@Link482b@@QAEHHHH@Z
int Link482b::send(int a, int b, int c)
{
    if (m_4 == 0)
        return 0;
    return m_4->send(a, b, this, c);
}
