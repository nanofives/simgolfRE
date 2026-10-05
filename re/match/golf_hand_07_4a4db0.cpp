// FLAGS golf_clean.exe: /O2 /GX
#include <stdlib.h>
#include "golf_hand_07_hdr.h"
struct N4a4db0 { int m_0, m_4; void* m_buf; unsigned m_size; N4a4db0* m_next; };
struct L4a4db0 {
    int m_0;
    N4a4db0* m_head;
    N4a4db0* m_tail;
    int m_count;
    int m_max;
    void removeHead();   // 0x4a4ed0
    int push(void** out, unsigned size, int a, int b);
};
// MATCH: golf_clean.exe 0x004a4db0 ?push@L4a4db0@@QAEHPAPAXIHH@Z
int L4a4db0::push(void** out, unsigned size, int a, int b)
{
    if (!out)
        return 3;
    *out = 0;
    if (m_count == m_max)
        return 0x12;
    if (!m_head) {
        m_head = (N4a4db0*)malloc(sizeof(N4a4db0));
        if (!m_head)
            return 4;
        m_tail = m_head;
    } else {
        m_tail->m_next = (N4a4db0*)malloc(sizeof(N4a4db0));
        if (!m_tail->m_next)
            return 4;
        m_tail = m_tail->m_next;
    }
    m_tail->m_next = 0;
    m_tail->m_4 = b;
    m_tail->m_0 = a;
    if (size) {
        m_tail->m_buf = malloc(size);
        if (!m_tail->m_buf) {
            removeHead();
            return 4;
        }
        m_tail->m_size = size;
    }
    m_count++;
    *out = m_tail->m_buf;
    return 0;
}
