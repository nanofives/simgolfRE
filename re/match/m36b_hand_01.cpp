// FLAGS sound.dll: /O2
// sound.dll hand matches (round 36, batch m36b): small Ctrl / stream / list helpers.
// Class and member names follow re/names (sound_*.tsv); layouts are the offsets the instructions use.

struct PtrNode { PtrNode *prev; PtrNode *next; void *val; };

struct PtrList {
    PtrNode *head;      // +0
    PtrNode *tail;      // +4
    PtrNode *cur;       // +8
    int count;          // +c
    void pushBack(void *p);
    PtrList();
    void *first() { cur = head; return cur ? cur->val : 0; }
    void *next() { if (!cur) return 0; cur = cur->next; return cur ? cur->val : 0; }
};

struct Elem { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4();
              virtual void v5(); virtual void slot18(); };

struct Ctrl {
    virtual char accept(void *p);
    int m4, m8;
    PtrList list;       // +c
    int appendIfAccepted(void *p);
    void callSlot18();
};

// MATCH: sound.dll 0x1001b9a0 ?appendIfAccepted@Ctrl@@QAEHPAX@Z
int Ctrl::appendIfAccepted(void *p)
{
    if (!accept(p))
        return 0x21;
    list.pushBack(p);
    return 0;
}

// MATCH: sound.dll 0x1001bc00 ?callSlot18@Ctrl@@QAEXXZ
void Ctrl::callSlot18()
{
    for (Elem *e = (Elem *)list.first(); e; e = (Elem *)list.next())
        e->slot18();
}

// MATCH: sound.dll 0x1001d130 ??0PtrList@@QAE@XZ
PtrList::PtrList()
{
    head = 0; tail = 0; cur = 0; count = 0;
}

struct Stream {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4();
    virtual void refill();      // +14
    char pad4[0x15];
    unsigned char last;         // +19
    char pad1a[0x12];
    unsigned char *buf;         // +2c
    unsigned int pos;           // +30
    int pad34, pad38;
    unsigned int end;           // +3c
    int readByte(int unused);
    void consumeByte();
};

// MATCH: sound.dll 0x1001c7a0 ?readByte@Stream@@QAEHH@Z
int Stream::readByte(int)
{
    if (pos >= end)
        refill();
    last = buf[pos++];
    return last;
}

// MATCH: sound.dll 0x1001c7e0 ?consumeByte@Stream@@QAEXXZ
void Stream::consumeByte()
{
    if (pos >= end)
        refill();
    last = buf[pos++];
}

// MATCH: sound.dll 0x1001e710 ?Midi_readVarLen@@YAIPAEPAH@Z
unsigned int Midi_readVarLen(unsigned char *p, int *len)
{
    unsigned int v = 0;
    *len = 0;
    do {
        v = (v << 7) | (*p & 0x7f);
        (*len)++;
    } while (*p++ & 0x80);
    return v;
}

struct Track {
    char pad[0x2c];
    int pos;                    // +2c
    int pad30;
    unsigned int flags;         // +34
    int pad38, pad3c;
    unsigned int off;           // +40
    unsigned int len;           // +44
    int advancePos(int n);
};

// MATCH: sound.dll 0x1001e9f0 ?advancePos@Track@@QAEHH@Z
int Track::advancePos(int n)
{
    if (off + n > len)
        return 0x22;
    if (off + n == len)
        flags |= 2;
    pos += n;
    off += n;
    return 0;
}

struct NodeListNode { int a, b, c; NodeListNode(int v); };

// MATCH: sound.dll 0x10025010 ??0NodeListNode@@QAE@H@Z
NodeListNode::NodeListNode(int v)
{
    a = 0; b = 0; c = v;
}
