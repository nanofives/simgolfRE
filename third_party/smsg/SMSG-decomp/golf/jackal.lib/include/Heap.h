#ifndef HEAP_H
#define HEAP_H

struct Heap {
    Heap();
    virtual ~Heap();
    void clear();
    void shutdown();
    int init(unsigned int bytes);
    void *Alloc(unsigned int n);

    char m_noGrow;
    void *m_block;
    char *m_cursor;
    int m_capacity;
    int m_avail;
};

#endif
