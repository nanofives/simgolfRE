// FLAGS golf_clean.exe: /O2 /GX
#include <stdlib.h>
struct Node4a0600 { int m_0; int m_data; int m_8; Node4a0600* m_next; Node4a0600* m_prev; };
struct O4a0600 {
    int m_0, m_4;
    Node4a0600* m_head;
    Node4a0600* m_cur;
    int m_count;
    int m_index;
    int m_18;
    char* m_1c;
    int m_20;
    int m_24;
    int run(int);   // 0x401d10
    int seek(int i)
    {
        if (i > m_count - 1)
            return 0;
        m_cur = m_head;
        if (i < 0) {
            int n = abs(i);
            if (n > m_count)
                return 0;
            while (n > 0) {
                m_cur = m_cur->m_prev;
                n--;
            }
            i = m_count + i;
        } else {
            int n = i;
            while (n > 0) {
                m_cur = m_cur->m_next;
                n--;
            }
        }
        m_index = i;
        return 1;
    }
    int post(char* s, int p, int lvl, int k);
};
// MATCH: golf_clean.exe 0x004a0600 ?post@O4a0600@@QAEHPADHHH@Z
int O4a0600::post(char* s, int p, int lvl, int k)
{
    int i, t;
    Node4a0600 *a, *b;
    m_1c = s;
    m_20 = lvl;
    m_24 = k;
    int r = run(p);
    if (r == 0) {
        for (i = m_count - 1; i > 0; i--) {
            if (seek(i)) {
                a = m_cur;
                if (seek((i + 1) % m_count)) {
                    b = m_cur;
                    a->m_data ^= b->m_data;
                    b->m_data ^= a->m_data;
                    a->m_data ^= b->m_data;
                    t = a->m_8;
                    a->m_8 = b->m_8;
                    b->m_8 = t;
                }
            }
        }
        }
    return r;
}
