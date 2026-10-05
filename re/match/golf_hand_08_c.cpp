// FLAGS golf_clean.exe: /O2 /GX
// probe
#include <stdlib.h>
#include <string.h>
#include <windows.h>
struct Node4a4b60 { int m_0, m_4, m_8; void* m_buf; unsigned m_size; Node4a4b60* m_next; };
struct List4a4b60 {
    int m_0;
    Node4a4b60* m_head;
    Node4a4b60* m_tail;
    int m_count;
    CRITICAL_SECTION m_cs;
    int add(const void* data, unsigned size, int a, int b, int c);
};
// MATCH: golf_clean.exe 0x004a4b60 ?add@List4a4b60@@QAEHPBXIHHH@Z
int List4a4b60::add(const void* data, unsigned size, int a, int b, int c)
{
    CRITICAL_SECTION* cs = &m_cs;
    EnterCriticalSection(cs);
    if (!m_head) {
        m_head = (Node4a4b60*)malloc(sizeof(Node4a4b60));
        if (!m_head) {
            LeaveCriticalSection(cs);
            return 4;
        }
        m_tail = m_head;
    } else {
        m_tail->m_next = (Node4a4b60*)malloc(sizeof(Node4a4b60));
        if (!m_tail->m_next) {
            LeaveCriticalSection(cs);
            return 4;
        }
        m_tail = m_tail->m_next;
    }
    m_tail->m_next = 0;
    m_tail->m_4 = a;
    m_tail->m_0 = b;
    m_tail->m_8 = c;
    if (size) {
        m_tail->m_buf = malloc(size);
        if (!m_tail->m_buf) {
            LeaveCriticalSection(cs);
            return 4;
        }
        if (data)
            memcpy(m_tail->m_buf, data, size);
        m_tail->m_size = size;
    }
    m_count++;
    LeaveCriticalSection(cs);
    return 0;
}
int clamp467130(int, int, int);
int dist467170(int, int);
// MATCH: golf_clean.exe 0x00435570 ?hit435570@@YAHHH@Z
int hit435570(int x, int y)
{
    int r = -1;
    if (x >= 0x136 && y >= 0x1f2)
    {
        r = clamp467130((y - 0x1f2) / 21, 0, 3);
        r += clamp467130((x - 0x136) / 121, 0, 3) * 4;
    }
    if (dist467170(x - 0x11d, y - 0x1ec) < 20)
        r = -2;
    if (dist467170(x - 0x100, y - 0x1fe) < 20)
        r = -3;
    if (dist467170(x - 0xe8, y - 0x21a) < 20)
        r = -4;
    if (dist467170(x - 0x14c, (y - 0x250) * 3) < 20)
        r = -5;
    if (dist467170(x - 0x304, (y - 0x250) * 3) < 20)
        r = -6;
    return r;
}
