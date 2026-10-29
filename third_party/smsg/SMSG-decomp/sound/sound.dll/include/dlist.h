#ifndef DLIST_H
#define DLIST_H

template <class T> class Node {
    Node<T> *m_pprev;
    Node<T> *m_pnext;
    T *m_pobj;

public:
    Node(T *pobj)
    {
        m_pprev = 0;
        m_pnext = 0;
        m_pobj = pobj;
    }
    Node<T> *get_prev() { return m_pprev; }
    void set_prev(Node<T> *n) { m_pprev = n; }
    Node<T> *get_next() { return m_pnext; }
    void set_next(Node<T> *n) { m_pnext = n; }
    T *get_obj() { return m_pobj; }
};

template <class T> class DList {
    Node<T> *m_phead;
    Node<T> *m_ptail;
    Node<T> *m_pcur;
    unsigned int m_nnodes;

public:
    DList()
    {
        m_phead = 0;
        m_ptail = 0;
        m_pcur = 0;
        m_nnodes = 0;
    }
    ~DList() { delete_all(); }
    T *get_pcur()
    {
        if (m_pcur)
            return m_pcur->get_obj();
        return 0;
    }
    void delete_all()
    {
        T *pobj = 0;
        pobj = remove_head();
        while (pobj)
            pobj = remove_head();
    }
    unsigned int get_nnodes() { return m_nnodes; }
    void add_to_end(T *new_obj)
    {
        Node<T> *new_node = new Node<T>(new_obj);
        if (m_ptail) {
            m_ptail->set_next(new_node);
            new_node->set_prev(m_ptail);
            m_ptail = new_node;
        } else {
            m_phead = new_node;
            m_ptail = new_node;
        }
        m_nnodes++;
    }
    void add_to_head(T *new_obj)
    {
        Node<T> *new_node = new Node<T>(new_obj);
        if (m_phead) {
            m_phead->set_prev(new_node);
            new_node->set_next(m_phead);
            m_phead = new_node;
        } else {
            m_phead = new_node;
            m_ptail = new_node;
        }
        m_nnodes++;
    }
    T *remove_head()
    {
        if (m_phead == 0)
            return 0;
        Node<T> *pcur = m_phead;
        Node<T> *pnext = pcur->get_next();
        if (pnext) {
            pnext->set_prev(0);
            m_phead = pnext;
        } else {
            m_ptail = 0;
            m_phead = 0;
        }
        T *pobj = pcur->get_obj();
        delete pcur;
        m_nnodes--;
        return pobj;
    }
    T *remove(T *pobj)
    {
        Node<T> *pcur = m_phead;
        while (pcur && pcur->get_obj() != pobj)
            pcur = pcur->get_next();
        if (pcur) {
            Node<T> *prev = pcur->get_prev();
            if (prev)
                prev->set_next(pcur->get_next());
            else
                m_phead = pcur->get_next();
            Node<T> *next = pcur->get_next();
            if (next) {
                next->set_prev(pcur->get_prev());
                m_pcur = next;
            } else {
                m_ptail = pcur->get_prev();
                m_pcur = 0;
            }
            T *pobj = pcur->get_obj();
            delete pcur;
            m_nnodes--;
            return pobj;
        }
        return 0;
    }
    T *remove_cur()
    {
        Node<T> *pcur = m_pcur;
        if (pcur) {
            Node<T> *prev = pcur->get_prev();
            if (prev)
                prev->set_next(pcur->get_next());
            else
                m_phead = pcur->get_next();
            Node<T> *next = pcur->get_next();
            if (next) {
                next->set_prev(pcur->get_prev());
                m_pcur = next;
            } else {
                m_ptail = pcur->get_prev();
                m_pcur = 0;
            }
            T *pobj = pcur->get_obj();
            delete pcur;
            m_nnodes--;
            return pobj;
        }
        return 0;
    }
    T *seek(T *pobj)
    {
        Node<T> *pcur = m_phead;
        while (pcur && pcur->get_obj() != pobj)
            pcur = pcur->get_next();
        if (pcur) {
            m_pcur = pcur;
            return pobj;
        }
        return 0;
    }
    Node<T> *get_head() { return m_phead; }
    T *get_first()
    {
        m_pcur = m_phead;
        if (m_pcur)
            return m_pcur->get_obj();
        return 0;
    }
    T *get_next()
    {
        if (m_pcur) {
            m_pcur = m_pcur->get_next();
            if (m_pcur)
                return m_pcur->get_obj();
        }
        return 0;
    }
    T *get_last()
    {
        m_pcur = m_ptail;
        if (m_pcur)
            return m_pcur->get_obj();
        return 0;
    }
    T *get_prev()
    {
        if (m_pcur) {
            m_pcur = m_pcur->get_prev();
            if (m_pcur)
                return m_pcur->get_obj();
        }
        return 0;
    }
    void insert(T *obj)
    {
        if (m_phead == 0)
            add_to_end(obj);
        else if (m_pcur == 0)
            add_to_head(obj);
        else {
            Node<T> *pnext = m_pcur->get_next();
            if (pnext == 0)
                add_to_end(obj);
            else {
                Node<T> *new_node = new Node<T>(obj);
                m_pcur->set_next(new_node);
                pnext->set_prev(new_node);
                new_node->set_prev(m_pcur);
                new_node->set_next(pnext);
                m_nnodes++;
            }
        }
    }
};

#endif
