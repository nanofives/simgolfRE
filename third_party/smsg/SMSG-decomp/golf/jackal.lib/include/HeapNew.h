#ifndef HEAPNEW_H
#define HEAPNEW_H

#include <windows.h>
#include "crt.h"
#include "Heap.h"

extern Heap *g_constructArena;

struct HeapNew {
    HeapNew()
    {
        m_arena = g_constructArena;
        g_constructArena = 0;
    }
    virtual ~HeapNew() { g_constructArena = m_arena; }
    void operator delete(void *p)
    {
        if (p) {
            if (g_constructArena == 0)
                free(p);
            g_constructArena = 0;
        }
    }
    void operator delete(void *p, Heap *)
    {
        if (p) {
            if (g_constructArena == 0)
                free(p);
            g_constructArena = 0;
        }
    }

    Heap *m_arena;
};

inline void *operator new(unsigned int n, Heap *a)
{
    void *p;
    if (a) {
        p = a->Alloc(n);
        if (p) {
            g_constructArena = a;
            return p;
        }
        MessageBoxA(0, "out of heap space in new function", "Aborting", 0);
        exit(3);
    }
    p = malloc(n);
    if (p == 0)
        return 0;
    g_constructArena = 0;
    return p;
}

#endif
