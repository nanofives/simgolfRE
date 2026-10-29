#include "jgldefs.h"
#include "crt.h"
#include <windows.h>
#include "StringUtil.h"
#include "Heap.h"
#include "HeapMessage.h"

#pragma intrinsic(strcat)

void Heap::clear()
{
    m_noGrow = 0;
    m_cursor = 0;
    m_block = 0;
    m_avail = 0;
    m_capacity = 0;
}

Heap::Heap()
{
    clear();
}

void Heap::shutdown()
{
    if (m_block) {
        FreeIfNotNull(m_block);
        m_block = 0;
    }
    clear();
}

Heap::~Heap()
{
    shutdown();
}

int Heap::init(unsigned int bytes)
{
    if (m_block)
        shutdown();
    m_block = AllocateBytes(bytes);
    if (!m_block)
        return 1;
    m_avail = bytes;
    m_capacity = bytes;
    m_cursor = (char *)m_block;
    return 0;
}

void *Heap::Alloc(unsigned int size)
{
    int n = (int)size;
    char *r;

    while (m_avail < n) {
        int step = 0x400;
        if (m_noGrow & 1)
            return 0;
        void *p = ReallocateBytes(m_block, m_capacity + step);
        if (p == 0) {
            ClearHeapMessage();
            AppendHeapMessage("Aborting due to a heap shortage!");
            add_lf(msg);
            AppendHeapMessage("Base size: ");
            AppendHeapMessageInt(m_capacity);
            add_lf(msg);
            AppendHeapMessage("Free size: ");
            AppendHeapMessageInt(m_avail);
            add_lf(msg);
            AppendHeapMessage("Requested size: ");
            AppendHeapMessageInt(n);
            MessageBoxA(0, msg, "Heap Notice!!", 0);
            exit(3);
        }
        m_block = p;
        m_capacity += step;
        m_avail += step;
    }
    r = m_cursor;
    m_cursor += n;
    m_avail -= n;
    return r;
}

Heap *g_constructArena;
