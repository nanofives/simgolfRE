#include "jgldefs.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "NetFifo.h"
#include "JackalUtil.h"

void fifo::init(int n)
{
    close();
    m_capacity = n;
}

void fifo::close()
{
    while (m_head != 0) {
        fifo_node *next = m_head->next;
        if (m_head->payload) {
            FreeIfNotNull(m_head->payload);
            m_head->payload = 0;
        }
        FreeIfNotNull(m_head);
        m_head = next;
    }
    m_tail = 0;
    m_count = 0;
}

int fifo::add(void **buffer, int size, int frame, int still)
{
    if (buffer == 0)
        return 3;
    *buffer = 0;
    if (m_count == m_capacity)
        return 0x12;
    if (m_head == 0) {
        m_head = (fifo_node *)AllocateBytes(sizeof(fifo_node));
        if (m_head == 0)
            return 4;
        m_tail = m_head;
    } else {
        m_tail->next = (fifo_node *)AllocateBytes(sizeof(fifo_node));
        if (m_tail->next == 0)
            return 4;
        m_tail = m_tail->next;
    }
    m_tail->next = 0;
    m_tail->still = still;
    m_tail->frame = frame;
    if (size != 0) {
        m_tail->payload = AllocateBytes(size);
        if (m_tail->payload == 0) {
            remove();
            return 4;
        }
        m_tail->payloadSize = size;
    }
    ++m_count;
    *buffer = m_tail->payload;
    return 0;
}

void *fifo::get(int *frame, int *still)
{
    if (m_head == 0)
        return 0;
    if (still)
        *still = m_head->still;
    if (frame)
        *frame = m_head->frame;
    return m_head->payload;
}

void fifo::remove()
{
    fifo_node *h;
    if (m_head == 0)
        return;
    h = m_head;
    if (h->payload) {
        FreeIfNotNull(h->payload);
        h->payload = 0;
    }
    m_head = m_head->next;
    FreeIfNotNull(h);
    --m_count;
}
