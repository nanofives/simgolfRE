#ifndef NETFIFO_H
#define NETFIFO_H

struct NetFifoNode {
    int to;
    int from;
    int result;
    void *payload;
    int payloadSize;
    NetFifoNode *next;
};

class NetFifo {
public:
    void close();
    int add(const void *data, int size, int from, int to, int result);
    int get(void *dest, int *from, int *to, int *size);

    int m_reserved;
    NetFifoNode *m_head;
    NetFifoNode *m_tail;
    int m_count;
    CRITICAL_SECTION m_lock;
};

struct fifo_node {
    int frame;
    int still;
    void *payload;
    int payloadSize;
    fifo_node *next;
};

class fifo {
public:
    void init(int n);
    void close();
    int add(void **buffer, int size, int frame, int still);
    void *get(int *frame, int *still);
    void remove();

    int m_reserved;
    fifo_node *m_head;
    fifo_node *m_tail;
    int m_count;
    int m_capacity;
};

#endif
