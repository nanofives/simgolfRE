// FLAGS sound.dll: /O2
// m36a (round 36): sound.dll list helpers matched by hand from the original instructions.

struct Node {
    Node *prev;
    Node *next;
    void *data;
};

struct NodeList {
    Node *head;
    Node *tail;
    int unk8;
    int count;
    void *popFront();
    void clear();
};

// MATCH: sound.dll 0x10005c00 ?popFront@NodeList@@QAEPAXXZ
void *NodeList::popFront()
{
    Node *n = head;
    if (n == 0)
        return 0;
    Node *nx = n->next;
    if (nx) {
        nx->prev = 0;
        head = nx;
    } else {
        tail = 0;
        head = 0;
    }
    void *d = n->data;
    delete n;
    count--;
    return d;
}

struct NodeListD : NodeList {
    inline void *pop()
    {
        Node *n = head;
        if (n == 0)
            return 0;
        Node *nx = n->next;
        if (nx) {
            nx->prev = 0;
            head = nx;
        } else {
            tail = 0;
            head = 0;
        }
        void *d = n->data;
        delete n;
        count--;
        return d;
    }
    void clearD();
};

// MATCH: sound.dll 0x10005a60 ?clearD@NodeListD@@QAEXXZ
void NodeListD::clearD()
{
    void *p = pop();
    while (p != 0)
        p = pop();
}

struct Triple {
    int a, b, c;
    Triple();
};

// MATCH: sound.dll 0x100087f0 ??0Triple@@QAE@XZ
Triple::Triple()
{
    a = 0;
    b = 0;
    c = 0;
}

struct Pair {
    int a, b;
    Pair(int x, int y);
};

// MATCH: sound.dll 0x10008810 ??0Pair@@QAE@HH@Z
Pair::Pair(int x, int y)
{
    a = x;
    b = y;
}

struct Seq {
    void scaleTickRate(int r);
};

struct Sound {
    char pad[0x60];
    Seq seq;
    int scaleTickRate(int r);
};

// MATCH: sound.dll 0x10005d80 ?scaleTickRate@Sound@@QAEHH@Z
int Sound::scaleTickRate(int r)
{
    seq.scaleTickRate(r);
    return 0;
}
