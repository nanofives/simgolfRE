inline SndNodeList::~SndNodeList()
{
    void *p = PopFront();
    while (p)
        p = PopFront();
}

inline int SndNodeList::Append(void *payload)
{
    SndNode *n = new SndNode(payload);
    if (m_tail) {
        m_tail->m_next = n;
        n->m_prev = m_tail;
        m_tail = n;
    } else {
        m_head = n;
        m_tail = n;
    }
    return ++m_count;
}

inline void *SndNodeList::Remove(void *payload)
{
    SndNode *p = m_head;
    while (p && p->m_payload != payload)
        p = p->m_next;
    if (p) {
        if (p->m_prev)
            p->m_prev->m_next = p->m_next;
        else
            m_head = p->m_next;
        SndNode *nx = p->m_next;
        if (nx) {
            nx->m_prev = p->m_prev;
            m_cursor = nx;
        } else {
            m_tail = p->m_prev;
            m_cursor = 0;
        }
        void *v = p->m_payload;
        delete p;
        m_count--;
        return v;
    }
    return 0;
}
