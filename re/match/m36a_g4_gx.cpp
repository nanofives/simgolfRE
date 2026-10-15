// FLAGS sound.dll: /O2 /GX
// m36a (round 36) group g4: sound.dll functions with an EH frame (/GX). Layouts as in m36a_g4.cpp.

typedef long HRES;

struct IDs {
    virtual HRES __stdcall QueryInterface(const void *iid, void **out) = 0;     // +0x00
    virtual unsigned long __stdcall AddRef() = 0;                              // +0x04
    virtual unsigned long __stdcall Release() = 0;                             // +0x08
    virtual HRES __stdcall CreateSoundBuffer(const void *desc, void **out, void *outer) = 0; // +0x0c
    virtual HRES __stdcall GetCaps(void *caps) = 0;                            // +0x10
    virtual HRES __stdcall DuplicateSoundBuffer(void *src, void **out) = 0;    // +0x14
};

int __cdecl mapDsError(HRES hr);
void *__cdecl operator new(unsigned int n);

struct Guid {
    unsigned long d[4];
};

extern "C" __declspec(dllimport) void __stdcall CoUninitialize();
void __cdecl operator delete(void *p);

struct LNode {
    LNode *prev;
    LNode *next;
    void *data;
    inline LNode(void *d)
    {
        prev = 0;
        next = 0;
        data = d;
    }
};

struct LList {
    LNode *head;            // +0x00
    LNode *tail;            // +0x04
    LNode *cursor;          // +0x08
    int count;              // +0x0c
    inline void append(void *d)
    {
        LNode *n = new LNode(d);
        if (tail) {
            tail->next = n;
            n->prev = tail;
        } else {
            head = n;
        }
        tail = n;
        count++;
    }
    inline void *pop()
    {
        LNode *n = head;
        if (n == 0)
            return 0;
        LNode *nx = n->next;
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
    inline ~LList()
    {
        void *p = pop();
        while (p != 0)
            p = pop();
    }
};

struct DsBuffer;

struct Channel {
    void registerBuffer(DsBuffer *b); // DsDevice_registerBuffer 0x32410
};

struct DsBuffer {
    char pad0[0x20];
    Channel *channel;       // +0x20
    char pad24[0x5c - 0x24];
    struct DirectSoundDevice *owner; // +0x5c
    void *dsb;              // +0x60 IDirectSoundBuffer
    char pad64[0xbc - 0x64];
    DsBuffer *prev;         // +0xbc
    DsBuffer *next;         // +0xc0
    char padc4[0xd4 - 0xc4];
    DsBuffer();             // DsBuffer::ctor 0xeef0
    ~DsBuffer();            // DsBuffer::destroy 0xef60
    bool hasAuxInterfaces(); // 0xfec0
    void query3D();         // 0xfef0
    void queryNotify();     // 0xff60
};

extern DsBuffer *g_dsBufferList;     // 0x100b4a34
extern DsBuffer *g_dsBufferListTail; // 0x100b4a38

inline void dsListAppend(DsBuffer *b)
{
    if (g_dsBufferListTail == 0) {
        g_dsBufferList = b;
        g_dsBufferListTail = b;
        b->prev = 0;
    } else {
        b->prev = g_dsBufferListTail;
        g_dsBufferListTail->next = b;
        g_dsBufferListTail = b;
    }
    b->next = 0;
}

struct DevNode {
    char *name;             // +0x00
    char *module;           // +0x04
    Guid *guid;             // +0x08
    struct DirectSoundDevice *owner; // +0x0c
    DevNode *next;          // +0x10
};

struct DirectSoundDevice {
    IDs *ds;                // +0x00
    DevNode *devHead;       // +0x04
    DevNode *devCursor;     // +0x08
    unsigned long caps[0x18]; // +0x0c
    unsigned char flags;    // +0x6c
    char pad6d[3];
    DsBuffer primary;       // +0x70
    char pad144[0x150 - 0x144];
    int maxBuffers;         // +0x150
    char pad154[0x15c - 0x154];
    int dupCount;           // +0x15c
    char pad160[0x168 - 0x160];
    LList list168;          // +0x168
    LList list178;          // +0x178
    char pad188[0x1b8 - 0x188];
    ~DirectSoundDevice();
    void releaseAll();      // 0x11970
    int duplicateBuffer(Channel *ch, DsBuffer *src, DsBuffer **out);
};

// DirectSoundDevice::close in the tracker: the destructor (EH states 2/1/0 for list178, list168, primary).
// MATCH: sound.dll 0x10011290 ??1DirectSoundDevice@@QAE@XZ
DirectSoundDevice::~DirectSoundDevice()
{
    DevNode *p = devHead;
    while (p) {
        if (p->name) {
            delete p->name;
            p->name = 0;
        }
        if (p->module) {
            delete p->module;
            p->module = 0;
        }
        if (p->guid) {
            delete p->guid;
            p->guid = 0;
        }
        DevNode *t = p;
        p = p->next;
        delete t;
    }
    devHead = 0;
    devCursor = 0;
    releaseAll();
    CoUninitialize();
}

// createBuffer / createBufferFmt: compiled from a source where removeBuffer's body calls unlinkDsBuffer and
// findAndUnlinkNode out of line and deletes through the scalar deleting destructor.
namespace m36a_12d10 {

struct DsBuf;

struct Channel {
    void voiceListRemove(DsBuf *b); // 0x32440
};

struct DsBuf {
    char pad0[0x20];
    Channel *channel;       // +0x20
    char pad24[0x5c - 0x24];
    struct Device *owner;   // +0x5c
    char pad60[0x74 - 0x60];
    unsigned long createFlags; // +0x74
    char pad78[0x98 - 0x78];
    unsigned long flags;    // +0x98
    char pad9c[0xbc - 0x9c];
    DsBuf *prev;            // +0xbc
    DsBuf *next;            // +0xc0
    char padc4[0xd4 - 0xc4];
    DsBuf();                // DsBuffer::ctor 0xeef0
    ~DsBuf();               // DsBuffer::destroy 0xef60
    int isBusy();           // 0x10e30
    int releaseIfOwned();   // 0x10060
    void *scalarDeletingDtor(unsigned int fl); // 0x12b80
    void setBufferSize(unsigned long size); // 0xf0c0
    void setPrimaryFormat(void *fmt); // 0xf990
    int create(IDs *ds, void *arg); // 0xfbb0
};

extern DsBuf *g_dsBufferList;     // 0x100b4a34
extern DsBuf *g_dsBufferListTail; // 0x100b4a38

struct BList {
    LNode *head;            // +0x00
    LNode *tail;            // +0x04
    LNode *cursor;          // +0x08
    int count;              // +0x0c
    void *findAndUnlinkNode(void *d); // 0x136c0
};

struct Device {
    IDs *ds;                // +0x00
    char pad4[0x6c - 4];
    unsigned char flags;    // +0x6c
    char pad6d[0x14c - 0x6d];
    unsigned int maxAll;    // +0x14c
    unsigned int free3d;    // +0x150
    unsigned int max3d;     // +0x154
    unsigned int count3d;   // +0x158
    unsigned int dupCount;  // +0x15c
    char pad160[0x164 - 0x160];
    void *enumArg;          // +0x164
    BList list168;          // +0x168
    int unlinkDsBuffer(DsBuf *b); // 0x134e0
    inline DsBuf *find(DsBuf *b)
    {
        for (DsBuf *p = g_dsBufferList; p; p = p->next)
            if (p == b)
                return p;
        return 0;
    }
    inline int remove(DsBuf *b)
    {
        if (!(flags & 2))
            return 3;
        if (b == 0)
            return 10;
        DsBuf *p = find(b);
        if (p)
            unlinkDsBuffer(p);
        if (b->flags | 4)
            count3d--;
        if (b->releaseIfOwned() == 0)
            b->scalarDeletingDtor(1);
        return 0;
    }
    inline int addToList(DsBuf *b)
    {
        if (g_dsBufferListTail == 0) {
            g_dsBufferList = b;
            g_dsBufferListTail = b;
            b->prev = 0;
            b->next = 0;
        } else {
            b->prev = g_dsBufferListTail;
            g_dsBufferListTail->next = b;
            g_dsBufferListTail = b;
            b->next = 0;
        }
        return 0;
    }
    int createBuffer(DsBuf **out, unsigned long fl, unsigned long size);
    int createBufferFmt(DsBuf **out, void *fmt, unsigned long fl, unsigned long size);
};

// MATCH: sound.dll 0x10012d10 ?createBuffer@Device@m36a_12d10@@QAEHPAPAUDsBuf@2@KK@Z
int Device::createBuffer(DsBuf **out, unsigned long fl, unsigned long size)
{
    if (!(flags & 2))
        return 3;
    unsigned long is3d = fl & 4;
    if (is3d) {
        unsigned int c = count3d;
        if (c >= max3d)
            return 0x25;
        count3d = ++c;
        unsigned int f = maxAll - c;
        free3d = f;
        if (dupCount > f) {
            DsBuf *b;
            LNode *n;
            for (n = list168.head; n; n = n->next) {
                b = (DsBuf *)n->data;
                if (!b->isBusy())
                    break;
            }
            DsBuf *d;
            if (n == 0 || (b->channel->voiceListRemove(b), (d = (DsBuf *)list168.findAndUnlinkNode(b)) == 0)) {
                count3d--;
                free3d = maxAll - count3d;
                return 0x25;
            }
            remove(d);
            dupCount--;
        }
    }
    *out = new DsBuf;
    (*out)->createFlags = fl;
    (*out)->setBufferSize(size);
    (*out)->owner = this;
    int r = (*out)->create(ds, enumArg);
    if (r == 0) {
        r = addToList(*out);
    } else {
        if (is3d) {
            count3d--;
            free3d = maxAll - count3d;
        }
        delete *out;
        *out = 0;
    }
    return r;
}

} // namespace m36a_12d10
