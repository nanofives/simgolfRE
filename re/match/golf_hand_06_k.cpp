// FLAGS golf_clean.exe: /O2 /GX
// probe
#include <stdlib.h>
#include <string.h>
#include <windows.h>
int dist40acd0(int dx, int dy);
struct Hole575 { int m_tx, m_ty; int pad[2]; int m_gx, m_gy; char pad2[0x208 - 0x18]; };
struct Pin59a { int m_x, m_y; int pad[4]; };
extern Hole575 g_575cc0[18];
extern Pin59a g_59aea8[18];
// MATCH: golf_clean.exe 0x00407340 ?nearestHole@@YAHHH@Z
int nearestHole(int x, int y)
{
    int best = 0xffff;
    int idx = -1;
    for (int i = 1; i < 19; i++) {
        int d = dist40acd0(g_575cc0[i - 1].m_tx - x, g_575cc0[i - 1].m_ty - y);
        if (d < best) {
            best = d;
            idx = i;
        }
        d = dist40acd0(g_575cc0[i - 1].m_gx - x, g_575cc0[i - 1].m_gy - y);
        if (d < best) {
            best = d;
            idx = i;
        }
        if (g_59aea8[i - 1].m_x != -1) {
            d = dist40acd0(x - (g_59aea8[i - 1].m_x >> 10), y - (g_59aea8[i - 1].m_y >> 10));
            if (d < best) {
                best = d;
                idx = i;
            }
        }
    }
    return idx;
}
struct Node4a4c { int m_0, m_4, m_8; void* m_buf; int m_size; Node4a4c* m_next; };
struct Queue4a4c {
    int m_0;
    Node4a4c* m_head;
    int m_8;
    int m_count;
    CRITICAL_SECTION m_cs;
    int pop(void* out, int* p4, int* p0, int* size);
};
// MATCH: golf_clean.exe 0x004a4c70 ?pop@Queue4a4c@@QAEHPAXPAH11@Z
int Queue4a4c::pop(void* out, int* p4, int* p0, int* size)
{
    CRITICAL_SECTION* cs = &m_cs;
    EnterCriticalSection(cs);
    if (!m_head) {
        LeaveCriticalSection(cs);
        return 0;
    }
    Node4a4c* n = m_head;
    if (size)
        *size = n->m_size;
    if (n->m_buf && out) {
        memcpy(out, n->m_buf, n->m_size);
        if (n->m_buf)
            free(n->m_buf);
        n->m_buf = 0;
    }
    m_head = m_head->m_next;
    if (p4)
        *p4 = n->m_4;
    if (p0)
        *p0 = n->m_0;
    int r = n->m_8;
    if (n)
        free(n);
    m_count--;
    LeaveCriticalSection(cs);
    return r;
}
