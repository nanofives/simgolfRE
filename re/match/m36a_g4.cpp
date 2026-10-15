// FLAGS sound.dll: /O2
// m36a (round 36) group g4: sound.dll DirectSound wrappers and list helpers (RVA 0x11000-0x13750), matched by hand
// from the original instructions. COM interfaces are declared as structs of __stdcall methods with padding slots.

typedef long HRES;

// IDirectSound3DBuffer: SetPosition is vtable slot 19 (+0x4c).
struct IDs3DBuffer {
    virtual HRES __stdcall s00() = 0; virtual HRES __stdcall s01() = 0; virtual HRES __stdcall s02() = 0;
    virtual HRES __stdcall s03() = 0; virtual HRES __stdcall s04() = 0; virtual HRES __stdcall s05() = 0;
    virtual HRES __stdcall s06() = 0; virtual HRES __stdcall s07() = 0; virtual HRES __stdcall s08() = 0;
    virtual HRES __stdcall s09() = 0; virtual HRES __stdcall s10() = 0; virtual HRES __stdcall s11() = 0;
    virtual HRES __stdcall s12() = 0; virtual HRES __stdcall s13() = 0; virtual HRES __stdcall s14() = 0;
    virtual HRES __stdcall s15() = 0; virtual HRES __stdcall s16() = 0; virtual HRES __stdcall s17() = 0;
    virtual HRES __stdcall s18() = 0;
    virtual HRES __stdcall SetPosition(float x, float y, float z, unsigned long apply) = 0;
};

// IDirectSound3DListener: SetPosition is vtable slot 14 (+0x38).
struct IDs3DListener {
    virtual HRES __stdcall s00() = 0; virtual HRES __stdcall s01() = 0; virtual HRES __stdcall s02() = 0;
    virtual HRES __stdcall s03() = 0; virtual HRES __stdcall s04() = 0; virtual HRES __stdcall s05() = 0;
    virtual HRES __stdcall s06() = 0; virtual HRES __stdcall s07() = 0; virtual HRES __stdcall s08() = 0;
    virtual HRES __stdcall s09() = 0; virtual HRES __stdcall s10() = 0; virtual HRES __stdcall s11() = 0;
    virtual HRES __stdcall s12() = 0; virtual HRES __stdcall s13() = 0;
    virtual HRES __stdcall SetPosition(float x, float y, float z, unsigned long apply) = 0;
};

// IDirectSound (dsound.h order).
struct IDs {
    virtual HRES __stdcall QueryInterface(const void *iid, void **out) = 0;     // +0x00
    virtual unsigned long __stdcall AddRef() = 0;                              // +0x04
    virtual unsigned long __stdcall Release() = 0;                             // +0x08
    virtual HRES __stdcall CreateSoundBuffer(const void *desc, void **out, void *outer) = 0; // +0x0c
    virtual HRES __stdcall GetCaps(void *caps) = 0;                            // +0x10
    virtual HRES __stdcall DuplicateSoundBuffer(void *src, void **out) = 0;    // +0x14
    virtual HRES __stdcall SetCooperativeLevel(void *hwnd, unsigned long level) = 0; // +0x18
    virtual HRES __stdcall Compact() = 0;                                      // +0x1c
    virtual HRES __stdcall GetSpeakerConfig(unsigned long *cfg) = 0;           // +0x20
    virtual HRES __stdcall SetSpeakerConfig(unsigned long cfg) = 0;            // +0x24
    virtual HRES __stdcall Initialize(const void *guid) = 0;                   // +0x28
};

struct Guid {
    unsigned long d[4];
};

int __cdecl mapDsError(HRES hr);
extern "C" char *__cdecl strncpy(char *dst, const char *src, unsigned int n);
extern "C" char *__cdecl strcpy(char *dst, const char *src);
extern "C" unsigned int __cdecl strlen(const char *s);
extern "C" void *__cdecl memset(void *p, int c, unsigned int n);
#pragma intrinsic(strcpy, strlen, memset)
extern "C" HRES __stdcall DirectSoundEnumerateA(int(__stdcall *cb)(Guid *, const char *,
    const char *, void *), void *ctx);
void *__cdecl operator new(unsigned int n);
void __cdecl operator delete(void *p);

// ---------------------------------------------------------------------------------------------------------------
// Ds3DBuffer: the DsBuffer fields of the 3D position (+0x64 IDirectSound3DBuffer, +0xc8 position).

struct Ds3DBuffer {
    char pad0[0x64];
    IDs3DBuffer *buf3d;     // +0x64
    char pad68[0xc8 - 0x68];
    float x;                // +0xc8
    float y;                // +0xcc
    float z;                // +0xd0
    int setPosition(float nx, float ny, float nz);
    int setPositionX(float v);
    int setPositionY(float v);
    int setPositionZ(float v);
};

// MATCH: sound.dll 0x10011000 ?setPosition@Ds3DBuffer@@QAEHMMM@Z
int Ds3DBuffer::setPosition(float nx, float ny, float nz)
{
    if (buf3d == 0)
        return 0xb;
    x = nx;
    y = ny;
    z = nz;
    return buf3d->SetPosition(nx, ny, nz, 0) ? 0x1f : 0;
}

// MATCH: sound.dll 0x10011070 ?setPositionX@Ds3DBuffer@@QAEHM@Z
int Ds3DBuffer::setPositionX(float v)
{
    if (buf3d == 0)
        return 0xb;
    x = v;
    buf3d->SetPosition(v, y, z, 0);
    return 0;
}

// MATCH: sound.dll 0x100110c0 ?setPositionY@Ds3DBuffer@@QAEHM@Z
int Ds3DBuffer::setPositionY(float v)
{
    if (buf3d == 0)
        return 0xb;
    y = v;
    buf3d->SetPosition(x, v, z, 0);
    return 0;
}

// MATCH: sound.dll 0x10011110 ?setPositionZ@Ds3DBuffer@@QAEHM@Z
int Ds3DBuffer::setPositionZ(float v)
{
    if (buf3d == 0)
        return 0xb;
    z = v;
    buf3d->SetPosition(x, y, v, 0);
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// Doubly linked list {head, tail, cursor, count} of {prev, next, data} nodes.

struct LNode {
    LNode *prev;            // +0x00
    LNode *next;            // +0x04
    void *data;             // +0x08
};

struct LList {
    LNode *head;            // +0x00
    LNode *tail;            // +0x04
    LNode *cursor;          // +0x08
    int count;              // +0x0c
    inline LList()
    {
        head = 0;
        tail = 0;
        cursor = 0;
        count = 0;
    }
    inline LNode *find(void *d)
    {
        for (LNode *n = head; n; n = n->next)
            if (n->data == d)
                return n;
        return 0;
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
    inline void clear()
    {
        void *p = pop();
        while (p != 0)
            p = pop();
    }
    inline LNode *findIdle();
    // Body of findAndUnlinkNode 0x136c0, inlined into freeBufferById.
    inline void *unlinkData(void *d)
    {
        LNode *n = find(d);
        if (n) {
            if (n->prev)
                n->prev->next = n->next;
            else
                head = n->next;
            LNode *nx = n->next;
            if (nx) {
                nx->prev = n->prev;
                cursor = nx;
            } else {
                tail = n->prev;
                cursor = 0;
            }
            void *r = n->data;
            delete n;
            count--;
            return r;
        }
        return 0;
    }
    void append(void *d);   // appendNode 0x137f0
    void *findAndUnlinkNode(void *d);
    void clearNodeListA();
    void clearNodeListB();
};

// MATCH: sound.dll 0x100136c0 ?findAndUnlinkNode@LList@@QAEPAXPAX@Z
void *LList::findAndUnlinkNode(void *d)
{
    LNode *n = find(d);
    if (n) {
        if (n->prev)
            n->prev->next = n->next;
        else
            head = n->next;
        LNode *nx = n->next;
        if (nx) {
            nx->prev = n->prev;
            cursor = nx;
        } else {
            tail = n->prev;
            cursor = 0;
        }
        void *r = n->data;
        delete n;
        count--;
        return r;
    }
    return 0;
}

// clearNodeListA and clearNodeListB are the same instructions (two copies of one source in the original).
// MATCH: sound.dll 0x10013620 ?clearNodeListA@LList@@QAEXXZ
void LList::clearNodeListA()
{
    void *p = pop();
    while (p != 0)
        p = pop();
}

// MATCH: sound.dll 0x10013750 ?clearNodeListB@LList@@QAEXXZ
void LList::clearNodeListB()
{
    void *p = pop();
    while (p != 0)
        p = pop();
}

// ---------------------------------------------------------------------------------------------------------------
// DsBuffer (0xd4 bytes) and the global buffer list.

struct DsBuffer;

struct Channel {
    char pad0[0x54];
    DsBuffer *source;       // +0x54
    void releaseBuffers();  // Channel_releaseBuffers 0x340b0
    void voiceListRemove(DsBuffer *b); // 0x32440
};

struct DsBuffer {
    char pad0[0x20];
    Channel *channel;       // +0x20
    char pad24[0x5c - 0x24];
    struct DirectSoundDevice *owner; // +0x5c
    char pad60[0x70 - 0x60];
    struct {                // +0x70 DSBUFFERDESC
        unsigned long dwSize;
        unsigned long dwFlags; // +0x74
        unsigned long dwBufferBytes;
        unsigned long dwReserved;
        void *lpwfxFormat;
    } desc;
    char pad84[0x98 - 0x84];
    unsigned long flags;    // +0x98
    char pad9c[0xbc - 0x9c];
    DsBuffer *prev;         // +0xbc
    DsBuffer *next;         // +0xc0
    char padc4[0xd4 - 0xc4];
    DsBuffer();             // DsBuffer::ctor 0xeef0
    ~DsBuffer();            // DsBuffer::destroy 0xef60
    int releaseIfOwned();   // 0x10060
    int release();          // 0xf8f0
    int isBusy();           // 0x10e30
    void setPrimaryFormat(void *fmt); // 0xf990
    int configureDesc(int primary); // 0x10f30
    int create(IDs *ds, void *arg); // 0xfbb0
};

struct WaveFmt {            // WAVEFORMATEX
    unsigned short tag;
    unsigned short channels;
    unsigned long rate;
    unsigned long avgBytes;
    unsigned short blockAlign;
    unsigned short bits;
    unsigned short cbSize;
};

struct WaveSrc {
    char pad0[0x64];
    char riff[4];           // +0x64
    void parseRiffWave(void *riff); // 0x326a0
};

struct Guid;
extern "C" __declspec(dllimport) HRES __stdcall CoInitialize(void *reserved);
extern "C" __declspec(dllimport) HRES __stdcall CoCreateInstance(const Guid *clsid, void *outer, unsigned long ctx,
    const Guid *iid, void **out);
extern const Guid CLSID_DirectSound; // 0x1005d67c
extern const Guid IID_IDirectSound;  // 0x1005d5dc

// First node whose buffer is not playing.
inline LNode *LList::findIdle()
{
    for (LNode *n = head; n; n = n->next)
        if (!((DsBuffer *)n->data)->isBusy())
            return n;
    return 0;
}

extern DsBuffer *g_dsBufferList;     // 0x100b4a34
extern DsBuffer *g_dsBufferListTail; // 0x100b4a38
extern void *g_dsCoopWindow;         // 0x100b4a3c
extern unsigned int g_dsDeviceIndex; // 0x100b4a40
extern int g_dsDeviceEnumCount;      // 0x100b4a44

// Body of unlinkDsBuffer 0x134e0, inlined into removeBuffer and stopAll.
inline int dsListUnlink(DsBuffer *b)
{
    if (b == 0)
        return 10;
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
    b->prev = 0;
    b->next = 0;
    return 0;
}

inline DsBuffer *dsListFind(DsBuffer *b)
{
    for (DsBuffer *p = g_dsBufferList; p; p = p->next)
        if (p == b)
            return p;
    return 0;
}

// ret 4 with ecx unused: __stdcall and a __thiscall member that ignores `this` compile identically (not determined).
// MATCH: sound.dll 0x100134e0 ?unlinkDsBuffer@@YGHPAUDsBuffer@@@Z
int __stdcall unlinkDsBuffer(DsBuffer *b)
{
    if (b == 0)
        return 10;
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
    b->prev = 0;
    b->next = 0;
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// DirectSoundDevice

struct DevNode {
    char *name;             // +0x00 description
    char *module;           // +0x04 driver module
    Guid *guid;             // +0x08
    struct DirectSoundDevice *owner; // +0x0c
    DevNode *next;          // +0x10
};

struct DirectSoundDevice {
    IDs *ds;                // +0x00
    DevNode *devHead;       // +0x04
    DevNode *devCursor;     // +0x08
    unsigned long caps[0x18]; // +0x0c DSCAPS (0x60 bytes); +0x54 = caps[0x12] dwFreeHwMemBytes
    unsigned char flags;    // +0x6c
    char pad6d[3];
    DsBuffer primary;       // +0x70
    int m144;               // +0x144
    unsigned int devCount;  // +0x148
    int m14c;               // +0x14c
    int m150;               // +0x150
    int m154;               // +0x154
    int bufferCount;        // +0x158
    int m15c;               // +0x15c
    int m160;               // +0x160
    void *enumArg;          // +0x164
    LList list168;          // +0x168
    LList list178;          // +0x178
    int m188;               // +0x188
    int releaseCount;       // +0x18c
    unsigned long coopLevel; // +0x190
    int m194;               // +0x194
    DsBuffer *aux;          // +0x198
    int m19c[4];            // +0x19c
    float lx, ly, lz;       // +0x1ac

    DirectSoundDevice();
    // Body of seekListToIndex 0x116a0, inlined into enumerate.
    inline int seek(unsigned int idx)
    {
        unsigned int n = devCount;
        if (n == 0)
            return 3;
        if (idx >= n)
            return 10;
        if (devCursor)
            return 6;
        DevNode *p = devHead;
        for (unsigned int i = 0; i < n; i++) {
            devCursor = p;
            if (i == idx)
                break;
            p = p->next;
        }
        return 0;
    }
    unsigned long getDeviceCaps();
    int setCooperativeLevel(void *hwnd);
    void releaseAll();
    void stopAll();
    // Body of removeBuffer 0x132e0, inlined into freeBufferById.
    inline int removeInline(DsBuffer *b)
    {
        if (!(flags & 2))
            return 3;
        if (b == 0)
            return 10;
        dsListUnlink(dsListFind(b));
        if (b->flags | 4) // `|` in the original (or eax, 4 at 0x1001336b): always true
            bufferCount--;
        if (b->releaseIfOwned() == 0)
            delete b;
        return 0;
    }
    int removeBuffer(DsBuffer *b);
    int releaseAllBuffers();
    int duplicateBuffer(Channel *ch, DsBuffer *src, DsBuffer **out); // 0x12260
    int freeBufferById(Channel *ch, DsBuffer **out);
    int create(void *hwnd, unsigned long level);
    int enumerate(unsigned int fl, void *arg);
    int seekListToIndex(unsigned int idx);
    int getNodeByIndex(unsigned int idx, char *dst, unsigned int n);
};

// initSoundVoice in the tracker: the constructor (member ctors of primary, list168, list178 run first).
// MATCH: sound.dll 0x10011180 ??0DirectSoundDevice@@QAE@XZ
DirectSoundDevice::DirectSoundDevice()
{
    releaseCount = 0;
    ds = 0;
    devHead = 0;
    devCursor = 0;
    memset(&flags, 0, 1);
    devCount = 0;
    m188 = 0x7f;
    bufferCount = 0;
    m15c = 0;
    m154 = 0;
    m14c = 0;
    m160 = 2;
    m194 = 0;
    aux = 0;
    lx = 0;
    ly = 0;
    lz = 0;
    memset(m19c, 0, sizeof(m19c));
    memset(caps, 0, sizeof(caps));
    m144 = 0;
}

// MATCH: sound.dll 0x10011760 ?getDeviceCaps@DirectSoundDevice@@QAEKXZ
unsigned long DirectSoundDevice::getDeviceCaps()
{
    if (ds) {
        ds->GetCaps(caps);
        return caps[0x12];
    }
    return 0;
}

// MATCH: sound.dll 0x100120a0 ?setCooperativeLevel@DirectSoundDevice@@QAEHPAX@Z
int DirectSoundDevice::setCooperativeLevel(void *hwnd)
{
    if (ds == 0)
        return 0x13;
    HRES hr = ds->SetCooperativeLevel(hwnd, coopLevel);
    if (hr) {
        mapDsError(hr);
        releaseAll();
        return 3;
    }
    g_dsCoopWindow = hwnd;
    return 0;
}

// MATCH: sound.dll 0x100132e0 ?removeBuffer@DirectSoundDevice@@QAEHPAUDsBuffer@@@Z
int DirectSoundDevice::removeBuffer(DsBuffer *b)
{
    if (!(flags & 2))
        return 3;
    if (b == 0)
        return 10;
    dsListUnlink(dsListFind(b));
    if (b->flags | 4) // `|` in the original (or eax, 4 at 0x1001336b): always true
        bufferCount--;
    if (b->releaseIfOwned() == 0)
        delete b;
    return 0;
}

// MATCH: sound.dll 0x10012690 ?freeBufferById@DirectSoundDevice@@QAEHPAUChannel@@PAPAUDsBuffer@@@Z
int DirectSoundDevice::freeBufferById(Channel *ch, DsBuffer **out)
{
    if (ch == 0)
        return 10;
    DsBuffer *b;
    LNode *n;
    for (n = list168.head; n; n = n->next) {
        b = (DsBuffer *)n->data;
        if (!b->isBusy())
            break;
    }
    if (n) {
        b->channel->voiceListRemove(b);
        DsBuffer *d = (DsBuffer *)list168.unlinkData(b);
        if (d) {
            removeInline(d);
            m15c--;
            int r = duplicateBuffer(ch, ch->source, out);
            if (r)
                *out = 0;
            else
                (*out)->channel = ch;
            return r;
        }
    }
    return 0x24;
}

// MATCH: sound.dll 0x10011a80 ?stopAll@DirectSoundDevice@@QAEXXZ
void DirectSoundDevice::stopAll()
{
    DsBuffer *b = g_dsBufferList;
    while (b) {
        if (b->channel) {
            list178.append(b->channel);
            b->channel->releaseBuffers();
        } else {
            dsListUnlink(b);
            b->release();
            delete b;
        }
        b = g_dsBufferList;
    }
    list168.clear();
    if (flags & 2)
        primary.releaseIfOwned();
    if (aux) {
        aux->releaseIfOwned();
        delete aux;
        aux = 0;
    }
    if (ds)
        ds->Release();
    ds = 0;
    releaseCount++;
}

// MATCH: sound.dll 0x100116a0 ?seekListToIndex@DirectSoundDevice@@QAEHI@Z
int DirectSoundDevice::seekListToIndex(unsigned int idx)
{
    unsigned int n = devCount;
    if (n == 0)
        return 3;
    if (idx >= n)
        return 10;
    if (devCursor)
        return 6;
    DevNode *p = devHead;
    for (unsigned int i = 0; i < n; i++) {
        devCursor = p;
        if (i == idx)
            break;
        p = p->next;
    }
    return 0;
}

// MATCH: sound.dll 0x10011790 ?getNodeByIndex@DirectSoundDevice@@QAEHIPADI@Z
int DirectSoundDevice::getNodeByIndex(unsigned int idx, char *dst, unsigned int n)
{
    if (idx >= devCount)
        return 10;
    DevNode *p = devHead;
    if (p == 0)
        return 3;
    for (unsigned int i = 0; i < idx && p; i++)
        p = p->next;
    if (p == 0)
        return 3;
    strncpy(dst, p->name, n);
    return 0;
}

int __stdcall dsEnumCallbackAppend(Guid *guid, const char *desc, const char *module, void *ctx);

// MATCH: sound.dll 0x100114e0 ?enumerate@DirectSoundDevice@@QAEHIPAX@Z
int DirectSoundDevice::enumerate(unsigned int fl, void *arg)
{
    if (ds || devHead || devCursor)
        return 0xc;
    if (arg == 0)
        return 10;
    enumArg = arg;
    if (fl & 0x20) {
        flags |= 8;
        if (fl & 0x40)
            flags |= 0x10;
    }
    HRES hr = DirectSoundEnumerateA(dsEnumCallbackAppend, this);
    if (hr) {
        mapDsError(hr);
        return 10;
    }
    int r = seek(g_dsDeviceIndex);
    if (r == 0)
        flags |= 1;
    return r;
}

// MATCH: sound.dll 0x10011810 ?dsEnumCallbackAppend@@YGHPAUGuid@@PBD1PAX@Z
int __stdcall dsEnumCallbackAppend(Guid *guid, const char *desc, const char *module, void *ctx)
{
    DevNode *n = (DevNode *)operator new(sizeof(DevNode));
    DirectSoundDevice *dev = (DirectSoundDevice *)ctx;
    dev->devCount++;
    DevNode *p = dev->devHead;
    if (p == 0) {
        dev->devHead = n;
    } else {
        while (p->next)
            p = p->next;
        p->next = n;
    }
    n->name = new char[strlen(desc) + 1];
    strcpy(n->name, desc);
    n->module = new char[strlen(module) + 1];
    strcpy(n->module, module);
    if (guid) {
        n->guid = new Guid;
        *n->guid = *guid;
    } else {
        n->guid = 0;
    }
    n->owner = dev;
    n->next = 0;
    g_dsDeviceEnumCount++;
    return 1;
}

struct Ds3DListener {
    char pad0[0xd8];
    IDs3DListener *listener; // +0xd8 (primary DsBuffer +0x68)
    char padd[0x1ac - 0xdc];
    float x;                 // +0x1ac
    float y;                 // +0x1b0
    float z;                 // +0x1b4
    int setPosition(float nx, float ny, float nz);
};

// MATCH: sound.dll 0x100135a0 ?setPosition@Ds3DListener@@QAEHMMM@Z
int Ds3DListener::setPosition(float nx, float ny, float nz)
{
    if (listener) {
        HRES hr = listener->SetPosition(nx, ny, nz, 0);
        if (hr)
            return mapDsError(hr);
    }
    x = nx;
    y = ny;
    z = nz;
    return 0;
}
