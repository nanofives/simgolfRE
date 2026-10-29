
#ifndef CLASSLIST_H
#define CLASSLIST_H

#include "HeapNew.h"
#include "LinkListEntry.h"

template <class T> struct ClassList : virtual HeapNew {
    LinkListEntry *m_head;
    LinkListEntry *m_cur;
    int m_count;
    int m_curIndex;
    Heap *m_pool;

    ClassList()
    {
        m_head = 0;
        m_cur = 0;
        m_count = 0;
        m_curIndex = 0;
        m_pool = 0;
    }
    ~ClassList() { Clear(); }
    int add(int key)
    {
        if (m_count == 0) {
            if (m_pool)
                m_head = new (m_pool) LinkListEntry;
            else {
                Heap *arena = m_arena;
                m_head = new (arena) LinkListEntry;
            }
            if (m_head == 0)
                return 4;
            m_head->key = key;
            m_head->next = m_head;
            m_head->prev = m_head;
            if (m_pool)
                m_head->elem = new (m_pool) T;
            else {
                Heap *arena = m_arena;
                m_head->elem = new (arena) T;
            }
            if (m_head->elem == 0)
                return 4;
            fill_entry(m_head);
            m_cur = m_head;
        } else {
            if (m_pool)
                m_head->prev->next = new (m_pool) LinkListEntry;
            else {
                Heap *arena = m_arena;
                m_head->prev->next = new (arena) LinkListEntry;
            }
            if (m_head->prev->next == 0)
                return 4;
            m_head->prev->next->prev = m_head->prev;
            m_head->prev->next->next = m_head;
            m_head->prev = m_head->prev->next;
            m_cur = m_head->prev;
            m_cur->key = key;
            if (m_pool)
                m_cur->elem = new (m_pool) T;
            else {
                Heap *arena = m_arena;
                m_cur->elem = new (arena) T;
            }
            if (m_cur->elem == 0)
                return 4;
            fill_entry(m_cur);
        }
        m_curIndex = m_count++;
        return 0;
    }
    T *next_entry()
    {
        if (m_head == 0)
            return 0;
        Advance();
        LinkListEntry *n = m_cur;
        T *e = (T *)n->elem;
        return e;
    }

    void remove(int key)
    {
        if (m_head == 0)
            return;
        for (int i = 0; i < m_count; i++) {
            if (m_cur->key == key) {
                m_cur->next->prev = m_cur->prev;
                m_cur->prev->next = m_cur->next;
                LinkListEntry *n = m_cur;
                if (n == m_head)
                    m_head = n->next;
                m_cur = n->next;
                T *e = (T *)n->elem;
                kill_entry(e);
                delete e;
                n->elem = 0;
                delete n;
                --m_count;
                break;
            }
            m_cur = m_cur->next;
        }
        if (m_count == 0)
            m_head = 0;
        m_curIndex = m_count - 1;
    }
    int GetCount() const { return m_count; }

    void remove_all()
    {
        if (m_head) {
            for (int i = 0; i < m_count; ++i) {
                LinkListEntry *n = m_head;
                m_cur = n->next;
                T *e = (T *)n->elem;
                kill_entry(e);
                delete e;
                m_head->elem = 0;
                delete m_head;
                m_head = m_cur;
            }
            m_head = 0;
            m_curIndex = 0;
            m_count = 0;
        }
    }
    void Clear()
    {
        remove_all();
        m_curIndex = 0;
    }
    int fill_entry(LinkListEntry *n) { return fill_entry((T *)n->elem); }

    int SetPos(int idx)
    {
        if (idx > m_count - 1)
            return 0;
        m_cur = m_head;
        if (idx < 0) {
            int n = abs(idx);
            if (n > m_count)
                return 0;
            while (n > 0) {
                m_cur = m_cur->prev;
                --n;
            }
            idx += m_count;
        } else {
            int n = idx;
            while (n > 0) {
                m_cur = m_cur->next;
                --n;
            }
        }
        m_curIndex = idx;
        return 1;
    }
    void Advance()
    {
        if (m_head) {
            m_cur = m_cur->next;
            if (++m_curIndex == m_count)
                m_curIndex = 0;
        }
    }
    T *current_entry()
    {
        if (m_head != 0)
            return (T *)m_cur->elem;
        return 0;
    }

    virtual int fill_entry(T *elem) { return 0; }
    virtual void kill_entry(T *elem) {}
};

#endif
