// FLAGS sound.dll: /O2 /GX
// m36a (round 36) second pass, group h2: 100% matches that need /GX.



namespace m36a_cd80 {
struct Node {
    Node *prev;
    Node *next;
    void *data;
    Node(void *d)
    {
        prev = 0;
        next = 0;
        data = d;
    }
};

struct DsBuffer {
    void setPosition();
};

struct Channel {
    char pad00[0x54];
    DsBuffer *dsbuf;     // +0x54 (sound +0xc4)
    unsigned flags;      // +0x58 (sound +0xc8)
    unsigned flags5c;    // +0x5c (sound +0xcc)
    char pad60[0x88 - 0x60];
    void setLoopFlag(int i);
    void closeDecoder();
    void servicePlayback();
    void updateVoiceRate();
    void recomputeGainsSetBit3(int ms);
};

#define V4(n) virtual void v##n##0(); virtual void v##n##4(); virtual void v##n##8(); virtual void v##n##c();
struct Sound {
    virtual ~Sound();                                          // slot 0
    virtual void v04(); virtual void v08(); virtual void v0c();
    virtual void v10(); virtual void v14(); virtual void v18(); virtual void v1c();
    virtual void v20(); virtual void v24(); virtual void v28(); virtual void v2c();
    virtual void v30();
    virtual void seek34(int a, int b, int c);                  // +0x34
    virtual void v38(); virtual void v3c();
    virtual void set40(int a);                                 // +0x40
    virtual void v44(); virtual void v48(); virtual void v4c();
    V4(5) V4(6) V4(7) V4(8) V4(9) V4(a) V4(b)
    virtual void vc0(); virtual void vc4(); virtual void vc8();
    virtual int isReadyCC();                                   // +0xcc
    char pad04[0x40];
    unsigned f44;
    char pad48[0x10];
    unsigned f58;
    char pad5c[0x14];
    Channel chan;              // +0x70
    Sound *subF8;              // +0xf8
    Sound *subFC;              // +0xfc
    char pad100[0x1420 - 0x100];
    void *hwnd1420;
    int f1424;
    unsigned char f1428;
    char pad1429[7];
    int f1430;
    int f1434;
    int count1438;
    int f143c;
    Sound *subs[1];            // +0x1440
    void freeBufferList();
};
#undef V4

struct CmdNode {
    int type;
    Sound *sound;
    int a, b;
    CmdNode();    // 0x100087f0 (listNode3Init)
    ~CmdNode();   // 0x10008840 (pairClear)
};

struct SoundList {
    Node *head;
    Node *tail;
    Node *cur;
    int count;
    __forceinline void append(void *d)
    {
        Node *nd = new Node(d);
        if (tail) {
            tail->next = nd;
            nd->prev = tail;
        } else
            head = nd;
        tail = nd;
        count++;
    }
    __forceinline Node *find(void *d)
    {
        Node *n;
        for (n = head; n; n = n->next)
            if (n->data == d)
                break;
        return n;
    }
    __forceinline void *remove(Node *n)
    {
        if (n == 0)
            return 0;
        if (n->prev)
            n->prev->next = n->next;
        else
            head = n->next;
        Node *nx = n->next;
        if (nx) {
            nx->prev = n->prev;
            cur = nx;
        } else {
            tail = n->prev;
            cur = 0;
        }
        void *d = n->data;
        delete n;
        count--;
        return d;
    }
};

struct CommandQueue {
    CmdNode *ring[0x1000];
    int readIdx;
    int writeIdx;
    CmdNode *at(unsigned i);
};

struct SoundThread {
    int postSoundThreadMessage(unsigned msg, unsigned wp, long lp);
};
extern SoundThread g_soundThread;    // 0x100b5038
extern "C" __declspec(dllimport) int __stdcall PostMessageA(void *hwnd, unsigned msg, unsigned wp, long lp);

struct Wave_Device {
    char pad00[0x1a4];
    unsigned flags1a4;
    char pad1a8[0x1c8 - 0x1a8];
    SoundList sounds;        // +0x1c8
    CommandQueue commands;   // +0x1d8
    __forceinline void stopSound(Sound *s)
    {
        Node *n = sounds.find(s);
        if (n)
            sounds.remove(n);
        Channel *ch = &s->chan;
        ch->flags |= 0x10;
        for (int i = 0; i < 16; i++)
            ch->setLoopFlag(i);
        ch->flags &= ~0x40;
    }
    __forceinline void post(Sound *s, CmdNode *c)
    {
        s->f1428 |= 1;
        if ((unsigned char)(flags1a4 >> 5) & 1)
            PostMessageA(s->hwnd1420, 0x7ee, 0, (long)c);
        else
            g_soundThread.postSoundThreadMessage(0x7ee, 0, (long)c);
    }
    void dispatchCommand();
};

// Case 17's first post is written out (not the inline post()): the flag byte goes through an `unsigned char` local
// and the window handle through a local; that gives the original's `shr ecx, 5; test cl, 1`, unhoisted pushes and the
// dl/edx registers of the second post. `unsigned f = flags1a4 >> 5; if ((unsigned char)f & 1)` compiles identically
// (source not determined there). The helpers need __forceinline: with plain inline VC6 stops inlining remove()/append()
// inside this function (16.1%).
// MATCH: sound.dll 0x1000cd80 ?dispatchCommand@Wave_Device@m36a_cd80@@QAEXXZ
void Wave_Device::dispatchCommand()
{
    unsigned r = commands.readIdx;
    unsigned w = commands.writeIdx;
    if (r == w)
        return;
    unsigned n = (w - r) & 0xfff;
    unsigned i = (r + 1) & 0xfff;
    CmdNode *c;
    Sound *s;
    Node *nd;
    for (unsigned k = 0; k < n; k++) {
        c = commands.at(i);
        if (c == 0)
            continue;
        switch (c->type) {
        case 1:
            s = c->sound;
            if (s->f44 & 4) {
                if (!(s->chan.flags & 0x40) && !(s->f1428 & 1)) {
                    sounds.append(s);
                    Channel *ch = &c->sound->chan;
                    ch->flags |= 0x40;
                    if ((c->sound->f58 & 2) && c->sound->isReadyCC() && (c->sound->chan.flags & 1))
                        c->sound->chan.servicePlayback();
                }
            } else if ((s->f44 & 0x100) && !(s->f58 & 2)) {
                sounds.append(s);
                c->sound->f58 |= 2;
            }
            delete c;
            break;
        case 2:
            s = c->sound;
            if (s->f44 & 4) {
                stopSound(s);
                if (s->f58 & 2) {
                    c->type = 3;
                    s = c->sound;
                    if (s)
                        post(s, c);
                } else {
                    if (s->chan.dsbuf) {
                        s->chan.closeDecoder();
                        s->chan.dsbuf->setPosition();
                    }
                    c->type = 5;
                    s = c->sound;
                    if (s)
                        post(s, c);
                }
            } else if (s->f44 & 0x100) {
                nd = sounds.find(s);
                if (nd)
                    sounds.remove(nd);
                c->sound->f58 &= ~3;
                delete c;
            }
            break;
        case 3:
            s = c->sound;
            if (s->f44 & 4) {
                stopSound(s);
                for (int j = 1; j < s->count1438; j++) {
                    Sound *sub = s->subs[j];
                    if (sub)
                        stopSound(sub);
                }
                s = c->sound;
                if (s)
                    post(s, c);
            } else if (s->f44 & 0x100) {
                nd = sounds.find(s);
                if (nd)
                    sounds.remove(nd);
                s->f58 &= ~3;
                s->freeBufferList();
            }
            break;
        case 4:
            s = c->sound;
            if (s->f44 & 4) {
                if (s) {
                    if (s->chan.flags & 0x40)
                        stopSound(s);
                    s = c->sound;
                    if (s)
                        post(s, c);
                }
            } else if (s->f44 & 0x100) {
                if (s->f58 & 2) {
                    nd = sounds.find(s);
                    if (nd)
                        sounds.remove(nd);
                }
                delete c->sound;
                delete c;
            }
            break;
        case 6:
            s = c->sound;
            if (s && (s->chan.flags5c & 4))
                s->chan.updateVoiceRate();
            delete c;
            break;
        case 13:
            s = c->sound;
            if (s) {
                s->f1430 = c->a;
                s->f1434 = c->b;
            }
            delete c;
            break;
        case 14:
            if (c->sound && c->sound->subF8) {
                if (c->a == 0)
                    c->sound->subF8->seek34(0, -1, c->b);
                else
                    c->sound->subF8->seek34(-1, 0, c->b);
            }
            delete c;
            break;
        case 15:
            if (c->sound && c->sound->subF8)
                c->sound->subF8->set40(c->a);
            delete c;
            break;
        case 16:
            if (c->sound && c->sound->subF8)
                c->sound->subF8->chan.recomputeGainsSetBit3(2000);
            delete c;
            break;
        case 17: {
            Sound *p = c->sound;
            if (p) {
                s = p->subF8;
                if (s) {
                    if (s->chan.flags & 0x40)
                        stopSound(s);
                    CmdNode *m = new CmdNode;
                    m->type = 4;
                    m->sound = s;
                    s->f1428 |= 1;
                    unsigned char f = (unsigned char)(flags1a4 >> 5);
                    if (f & 1) {
                        void *h = s->hwnd1420;
                        PostMessageA(h, 0x7ee, 0, (long)m);
                    } else
                        g_soundThread.postSoundThreadMessage(0x7ee, 0, (long)m);
                }
                s = p->subFC;
                if (s) {
                    CmdNode *m = new CmdNode;
                    m->type = 4;
                    m->sound = s;
                    post(s, m);
                }
                delete p;
            }
            delete c;
            break;
        }
        }
        commands.ring[i] = 0;
        i = (i + 1) & 0xfff;
    }
    commands.readIdx = w;
}
}

// ---- DirectSoundDevice (types copied from re/match/m36a_g4_gx.cpp) ----
typedef long HRES;
void *__cdecl operator new(unsigned int n);
void __cdecl operator delete(void *p);

namespace m36a_12260 {
// m36a (round 36) group g4: sound.dll functions with an EH frame (/GX). Layouts as in m36a_g4.cpp.


struct IDs {
    virtual HRES __stdcall QueryInterface(const void *iid, void **out) = 0;     // +0x00
    virtual unsigned long __stdcall AddRef() = 0;                              // +0x04
    virtual unsigned long __stdcall Release() = 0;                             // +0x08
    virtual HRES __stdcall CreateSoundBuffer(const void *desc, void **out, void *outer) = 0; // +0x0c
    virtual HRES __stdcall GetCaps(void *caps) = 0;                            // +0x10
    virtual HRES __stdcall DuplicateSoundBuffer(void *src, void **out) = 0;    // +0x14
};

int __cdecl mapDsError(HRES hr);

struct Guid {
    unsigned long d[4];
};


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

// Exits merged into one `return r` (that gives the late `push edi` after the new-expression and keeps 0 in ebp); `src`
// declared `DsBuffer *volatile` keeps it on the stack as in the original (with a plain pointer VC6 holds it in ebp and
// moves 0 to ebx, 73.1%). The volatile spelling reproduces the stack reads; how the original source kept src in memory
// is not determined.
// MATCH: sound.dll 0x10012260 ?duplicateBuffer@DirectSoundDevice@m36a_12260@@QAEHPAUChannel@2@PAUDsBuffer@2@PAPAU42@@Z
int DirectSoundDevice::duplicateBuffer(Channel *ch, DsBuffer *volatile src, DsBuffer **out)
{
    if (!(flags & 2) || ds == 0)
        return 3;
    if (dupCount == maxBuffers)
        return 0x25;
    *out = new DsBuffer;
    (*out)->owner = this;
    HRES hr = ds->DuplicateSoundBuffer(src->dsb, &(*out)->dsb);
    int r;
    if (hr == 0) {
        dsListAppend(*out);
        list168.append(*out);
        (*out)->channel = ch;
        ch->registerBuffer(*out);
        if (src->hasAuxInterfaces()) {
            (*out)->query3D();
            (*out)->queryNotify();
        }
        dupCount++;
        r = 0;
    } else {
        mapDsError(hr);
        delete *out;
        *out = 0;
        r = 0x1d;
    }
    return r;
}
}

namespace m36a_12fe0 {
// m36a (round 36) group g4: sound.dll functions with an EH frame (/GX). Layouts as in m36a_g4.cpp.


struct IDs {
    virtual HRES __stdcall QueryInterface(const void *iid, void **out) = 0;     // +0x00
    virtual unsigned long __stdcall AddRef() = 0;                              // +0x04
    virtual unsigned long __stdcall Release() = 0;                             // +0x08
    virtual HRES __stdcall CreateSoundBuffer(const void *desc, void **out, void *outer) = 0; // +0x0c
    virtual HRES __stdcall GetCaps(void *caps) = 0;                            // +0x10
    virtual HRES __stdcall DuplicateSoundBuffer(void *src, void **out) = 0;    // +0x14
};

int __cdecl mapDsError(HRES hr);

struct Guid {
    unsigned long d[4];
};


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

// is3d is computed through a volatile read of fl's address: fl becomes address-taken, so is3d lives on the stack and
// ebp holds 0 for the whole function as in the original (plain `fl & 4`: is3d in ebp, 65.4%; a volatile fl parameter
// keeps the order of its later load and leaves one instruction out of place, 99.4%). How the original source kept is3d
// in memory is not determined.
// MATCH: sound.dll 0x10012fe0 ?createBufferFmt@Device@m36a_12fe0@@QAEHPAPAUDsBuf@2@PAXKK@Z
int Device::createBufferFmt(DsBuf **out, void *fmt, unsigned long fl, unsigned long size)
{
    if (!(flags & 2))
        return 3;
    if (fmt == 0)
        return 10;
    int r;
    unsigned long is3d = *(volatile unsigned long *)&fl & 4;
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
    (*out)->setPrimaryFormat(fmt);
    (*out)->owner = this;
    r = (*out)->create(ds, enumArg);
    if (r == 0) {
        addToList(*out);
    } else {
        if (is3d) {
            unsigned int c = count3d - 1;
            free3d = maxAll - c;
            count3d = c;
        }
        delete *out;
        *out = 0;
    }
    return r;
}
}
