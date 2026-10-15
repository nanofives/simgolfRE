// FLAGS sound.dll: /O2
// m36a (round 36) group h1: second pass over near misses of g1..g4 (sound.dll), matched by hand.
typedef long HRES;
void __cdecl operator delete(void *p);

struct IDs {
    virtual HRES __stdcall QueryInterface(const void *iid, void **out) = 0;
    virtual unsigned long __stdcall AddRef() = 0;
    virtual unsigned long __stdcall Release() = 0;
};

// ---------------------------------------------------------------------------------------------------------------
// DirectSoundDevice::releaseAll / releaseAllBuffers. The inlined list unlink clears b->prev through a pointer to the
// field (`DsBuffer **pp = &b->prev; *pp = 0;`): VC6 then keeps that store ahead of `mov ecx, esi` and the b->next
// store after it, as the original does. With two plain member stores `mov ecx, esi` goes before both (98.5%).
// What the source wrote there (a helper taking DsBuffer **, a macro) is not determined; the pointer store is the
// spelling that compiles to the original.

namespace m36a_dsrel {
struct DsBuffer {
    char pad0[0xbc];
    DsBuffer *prev;         // +0xbc
    DsBuffer *next;         // +0xc0
    char padc4[0xd4 - 0xc4];
    ~DsBuffer();            // DsBuffer::destroy 0xef60
    int releaseIfOwned();   // 0x10060
};
extern DsBuffer *g_dsBufferList;     // 0x100b4a34
extern DsBuffer *g_dsBufferListTail; // 0x100b4a38

struct DirectSoundDevice {
    IDs *ds;                // +0x00
    void *devHead;          // +0x04
    void *devCursor;        // +0x08
    unsigned long caps[0x18]; // +0x0c
    unsigned char flags;    // +0x6c
    char pad6d[3];
    DsBuffer primary;       // +0x70
    char pad144[0x18c - 0x144];
    int releaseCount;       // +0x18c
    char pad190[0x198 - 0x190];
    DsBuffer *aux;          // +0x198
    void releaseAll();
    int releaseAllBuffers();
};

// MATCH: sound.dll 0x10011970 ?releaseAll@DirectSoundDevice@m36a_dsrel@@QAEXXZ
void DirectSoundDevice::releaseAll()
{
    DsBuffer *b;
    while ((b = g_dsBufferList) != 0) {
        DsBuffer *p = b->prev;
        DsBuffer *n = b->next;
        if (p)
            p->next = n;
        else
            g_dsBufferList = n;
        if (n)
            n->prev = p;
        else
            g_dsBufferListTail = p;
        DsBuffer **pp = &b->prev;
        *pp = 0;
        b->next = 0;
        b->releaseIfOwned();
        delete b;
    }
    if (flags & 2)
        primary.releaseIfOwned();
    if (aux) {
        aux->releaseIfOwned();
        delete aux;
        aux = 0;
    }
    if (ds) {
        ds->Release();
        ds = 0;
    }
    devCursor = 0;
    releaseCount++;
}

// MATCH: sound.dll 0x100133d0 ?releaseAllBuffers@DirectSoundDevice@m36a_dsrel@@QAEHXZ
int DirectSoundDevice::releaseAllBuffers()
{
    if (!(flags & 2))
        return 3;
    DsBuffer *b;
    while ((b = g_dsBufferList) != 0) {
        DsBuffer *p = b->prev;
        DsBuffer *n = b->next;
        if (p)
            p->next = n;
        else
            g_dsBufferList = n;
        if (n)
            n->prev = p;
        else
            g_dsBufferListTail = p;
        DsBuffer **pp = &b->prev;
        *pp = 0;
        b->next = 0;
        b->releaseIfOwned();
        delete b;
    }
    return 0;
}
} // namespace m36a_dsrel
