#include <crtdbg.h>
#include "jgllist.h"
#include "gsobject.h"
#include "jglwin.h"
#include "jglglobals.h"
LinkedList GSObject::object_list;
GSObject::GSObject()
{
    object_list.add(this, 0);
}

GSObject::~GSObject()
{
    object_list.Select((int)this);
    object_list.remove_current();
}

void DestroyAllSprites(void)
{
    while (GSObject::object_list.Count())
        delete (GSObject *)GSObject::object_list.m_curId;
}

LinkedList::LinkedList()
{
    m_head = 0;
    m_cur = 0;
    m_count = 0;
}

int LinkedList::Count()
{
    return this->m_count;
}

void LinkedList::close()
{
    Node *p;
    Node *q;
    if (m_head == 0)
        return;
    q = m_head->m_prev;
    for (p = q->m_prev; p != m_head; p = q->m_prev) {
        q->m_prev = p->m_prev;
        delete p;
    }
    delete m_head;
    m_flags = 0;
    m_count = 0;
    m_head = 0;
    m_cur = 0;
}

void LinkedList::add(void *item, uchar flag)
{
    if (m_count != 0 && m_flags != 0)
        OutputDebugStringA("BAD USE OF LINKED LIST - Ordered or unordered\n");
    m_flags = 0;
    Node *n = new Node(item, flag);
    if (m_head != 0) {
        n->m_next = m_head->m_next;
        n->m_prev = m_head;
        m_head->m_next = n;
        n->m_next->m_prev = n;
    } else {
        m_head = n;
        m_head->m_next = m_head;
        m_head->m_prev = m_head;
    }
    m_count = m_count + 1;
    m_cur = n;
    m_curId = (int)item;
    m_curKind = m_cur->m_kind;
}

void LinkedList::remove_current()
{
    Node *p;
    if (m_head == 0)
        return;
    if (m_head == m_head->m_next) {
        delete m_head;
        m_head = 0;
        m_cur = 0;
        m_count = 0;
        return;
    }
    if (m_head == m_cur)
        m_head = m_head->m_next;
    p = m_cur;
    m_cur = m_cur->m_next;
    m_curId = m_cur->m_id;
    m_curKind = m_cur->m_kind;
    p->m_prev->m_next = p->m_next;
    p->m_next->m_prev = p->m_prev;
    delete p;
    m_count = m_count - 1;
}

int LinkedList::Select(int id)
{
    Node *p;
    Node *tail;
    if (m_head == 0)
        return 0;
    tail = m_head->m_prev;
    p = m_head;
    do {
        if (p->m_id == id) {
            m_cur = p;
            m_curId = m_cur->m_id;
            m_curKind = m_cur->m_kind;
            return 1;
        }
        p = p->m_next;
    } while (p != m_head);
    return 0;
}

Node::Node(void *item, uchar kind)
{
    m_id = (int)item;
    m_kind = kind;
    m_prev = 0;
    m_next = 0;
}

Node::~Node()
{
    m_prev = 0;
    m_next = 0;
}
