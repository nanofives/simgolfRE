#ifndef JGLLIST_H
#define JGLLIST_H
#include "jgltypes.h"
struct Node {
    Node(void *item, uchar kind);
    virtual ~Node();
    Node         *m_next;
    Node         *m_prev;
    int           m_id;
    unsigned char m_kind;
};
class LinkedList {
public:
    LinkedList();
    virtual ~LinkedList()
    {
        close();
    }
    void  close();
    int   Count();
    void  remove_current();
    int   Select(int id);
    void  add(void *item, uchar flag);
    int   m_curId;
    uchar m_curKind;
    int   m_count;
    Node *m_head;
    Node *m_cur;
    int   m_flags;
};
#endif
