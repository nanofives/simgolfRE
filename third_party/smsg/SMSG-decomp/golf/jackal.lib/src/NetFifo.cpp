#include "jgldefs.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "NetFifo.h"
#include "JackalUtil.h"

void NetFifo::close()
{
    EnterCriticalSection(&m_lock);
    while (m_head) {
        NetFifoNode *next = m_head->next;
        if (m_head->payload)
            FreeIfNotNull(m_head->payload);
        m_head->payload = 0;
        FreeIfNotNull(m_head);
        m_head = next;
    }
    m_tail = 0;
    m_count = 0;
    LeaveCriticalSection(&m_lock);
}

int NetFifo::add(const void *data, int size, int from, int to, int result)
{
    EnterCriticalSection(&m_lock);
    if (m_head == 0) {
        m_head = (NetFifoNode *)AllocateBytes(sizeof(NetFifoNode));
        if (m_head == 0) {
            LeaveCriticalSection(&m_lock);
            return 4;
        }
        m_tail = m_head;
    } else {
        m_tail->next = (NetFifoNode *)AllocateBytes(sizeof(NetFifoNode));
        if (m_tail->next == 0) {
            LeaveCriticalSection(&m_lock);
            return 4;
        }
        m_tail = m_tail->next;
    }
    m_tail->next = 0;
    m_tail->from = from;
    m_tail->to = to;
    m_tail->result = result;
    if (size != 0) {
        m_tail->payload = AllocateBytes(size);
        if (m_tail->payload == 0) {
            LeaveCriticalSection(&m_lock);
            return 4;
        }
        if (data != 0)
            memcpy(m_tail->payload, data, size);
        m_tail->payloadSize = size;
    }
    ++m_count;
    LeaveCriticalSection(&m_lock);
    return 0;
}

int NetFifo::get(void *dest, int *from, int *to, int *size)
{
    NetFifoNode *node;
    int result;
    EnterCriticalSection(&m_lock);
    if (m_head == 0) {
        LeaveCriticalSection(&m_lock);
        return 0;
    }
    node = m_head;
    if (size)
        *size = node->payloadSize;
    if (node->payload != 0 && dest != 0) {
        memcpy(dest, node->payload, node->payloadSize);
        FreeIfNotNull(node->payload);
        node->payload = 0;
    }
    m_head = m_head->next;
    if (from)
        *from = node->from;
    if (to)
        *to = node->to;
    result = node->result;
    FreeIfNotNull(node);
    --m_count;
    LeaveCriticalSection(&m_lock);
    return result;
}
