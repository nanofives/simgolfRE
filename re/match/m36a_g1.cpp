// FLAGS sound.dll: /O2
// m36a (round 36) group g1: sound.dll functions 0x3f30..0x9e40 matched by hand from the original instructions.

#include <windows.h>
#include <string.h>
#include <stdio.h>

namespace m36a_5b60 {
struct Node {
    Node *prev;
    Node *next;
    void *data;
};

struct NodeListE {
    Node *head;
    Node *tail;
    int unk8;
    int count;
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
    void clearE();
};

// MATCH: sound.dll 0x10005b60 ?clearE@NodeListE@m36a_5b60@@QAEXXZ
void NodeListE::clearE()
{
    void *p = pop();
    while (p != 0)
        p = pop();
}
}

namespace m36a_6c70 {
struct Node {
    Node *prev;
    Node *next;
    void *data;
};

struct NodeListF {
    Node *head;
    Node *tail;
    int unk8;
    int count;
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
    void clearF();
};

// MATCH: sound.dll 0x10006c70 ?clearF@NodeListF@m36a_6c70@@QAEXXZ
void NodeListF::clearF()
{
    void *p = pop();
    while (p != 0)
        p = pop();
}
}

namespace m36a_9390 {
struct SoundTimer {
    int x;
    void stopKeepLock();
    void stop();
    ~SoundTimer() { stop(); }
};
extern SoundTimer *g_soundTimer;   // 0x100b49ec

// MATCH: sound.dll 0x10009390 release_sound
int release_sound()
{
    if (g_soundTimer) {
        g_soundTimer->stopKeepLock();
        delete g_soundTimer;
        g_soundTimer = 0;
        return 0;
    }
    return 0;
}
}

namespace m36a_5eb0 {
struct SNode {
    SNode *prev;
    SNode *next;
    void *data;
};
struct SList {
    SNode *head;
    SNode *tail;
    SNode *cur;
    int count;
};
extern SList g_soundList;   // 0x100b5000

struct Sound {
    int unregisterSound();
};

// MATCH: sound.dll 0x10005eb0 unregisterSound
int Sound::unregisterSound()
{
    SNode *n;
    for (n = g_soundList.head; n; n = n->next)
        if (n->data == this)
            break;
    if (n == 0)
        return 0;
    if (n->prev)
        n->prev->next = n->next;
    else
        g_soundList.head = n->next;
    SNode *nx = n->next;
    if (nx) {
        nx->prev = n->prev;
        g_soundList.cur = nx;
    } else {
        g_soundList.tail = n->prev;
        g_soundList.cur = 0;
    }
    delete n;
    g_soundList.count--;
    return 0;
}
}

namespace m36a_6bd0 {
struct Capture {
    char pad0[0xb4];
    unsigned m_b4;       // 0xb4
    char pad1[0xc0 - 0xb8];
    int m_c0;            // 0xc0
    int pad2;
    int m_c8;            // 0xc8
    char pad3[0xd4 - 0xcc];
    int m_d4;            // 0xd4
    int pad4;
    unsigned m_dc;       // 0xdc
    char pad5[0xe4 - 0xe0];
    unsigned short m_e4; // 0xe4
    void computeCaptureDuration();
};

// MATCH: sound.dll 0x10006bd0 computeCaptureDuration
void Capture::computeCaptureDuration()
{
    if (m_b4 && m_d4) {
        int unit = m_b4 / m_e4;
        m_c8 = m_d4 / unit;
        m_c0 = (int)((double)(m_d4 % unit) / (double)m_dc * 1000.0);
    }
}
}

namespace m36a_8ce0 {
void CALLBACK timerProc(UINT, UINT, DWORD, DWORD, DWORD);
extern int g_timerTickBusy;   // 0x100b49a8

struct SoundTimer {
    UINT delay;          // 0x00
    char pad0[0x18 - 4];
    UINT resolution;     // 0x18
    UINT eventFlags;     // 0x1c
    char pad1[0x3c - 0x20];
    int flags;           // 0x3c
    UINT id;             // 0x40
    int start();
};

// MATCH: sound.dll 0x10008ce0 ?start@SoundTimer@m36a_8ce0@@QAEHXZ
int SoundTimer::start()
{
    if (flags & 4)
        return 0xd;
    g_timerTickBusy = 0;
    if (timeBeginPeriod(resolution) == TIMERR_NOCANDO)
        return 0xb;
    id = timeSetEvent(delay, resolution, timerProc, (DWORD)this, eventFlags);
    if (id == TIMERR_NOCANDO)
        return 0xb;
    flags |= 7;
    return 0;
}
}

namespace m36a_6e00 {
struct StreamBuffer {
    int m_0;
    void *data;          // 0x04
    int owned : 1;       // 0x08
    int capacity;        // 0x0c
    int m_10;
    int m_14;
    int m_18;
    WAVEFORMATEX wf;     // 0x1c
    char pad[0x30 - 0x2e];
    int m_30;
    int m_34;
    int m_38;
    StreamBuffer();
};

// MATCH: sound.dll 0x10006e00 ??0StreamBuffer@m36a_6e00@@QAE@XZ
StreamBuffer::StreamBuffer()
{
    m_0 = 0;
    data = 0;
    m_10 = 2;
    m_14 = 0;
    capacity = 0x4000;
    data = operator new(0x20000);
    owned = 0;
    m_18 = 0;
    m_30 = 0;
    m_38 = 0;
    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = 1;
    wf.cbSize = 0;
    m_34 = 0;
    wf.nChannels = 2;
    wf.nSamplesPerSec = 22050;
    wf.nAvgBytesPerSec = 88200;
    wf.nBlockAlign = 4;
    wf.wBitsPerSample = 16;
}
}

namespace m36a_6420 {
extern int g_mmioOpenCount;   // 0x100b4a08
struct Wave_Device {
    char pad0[0x6c];
    MMIOINFO info;       // 0x6c (hmmio at 0xb0)
    int bufSize;         // 0xb4
    int defaultSize;     // 0xb8
    MMIOINFO *openMemoryMmio(int size);
};

// MATCH: sound.dll 0x10006420 ?openMemoryMmio@Wave_Device@m36a_6420@@QAEPAU_MMIOINFO@@H@Z
MMIOINFO *Wave_Device::openMemoryMmio(int size)
{
    if (info.hmmio)
        return 0;
    MMIOINFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.fccIOProc = FOURCC_MEM;
    mi.cchBuffer = size ? size : defaultSize;
    mi.adwInfo[0] = 100;
    HMMIO h = mmioOpen(NULL, &mi, MMIO_CREATE);
    if (h == 0)
        return 0;
    g_mmioOpenCount++;
    mmioGetInfo(h, &info, 0);
    return &info;
}
}

namespace m36a_8920 {
struct Node {
    Node *prev;
    Node *next;
    void *data;
};
struct List {
    Node *head;
    Node *tail;
    Node *cur;
    int count;
    inline void *removeNode(Node *n)
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
        void *r = n->data;
        delete n;
        count--;
        return r;
    }
};
extern CRITICAL_SECTION g_timerLock;   // 0x100b4990
struct Client {
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void release();
};
struct SoundTimer {
    char pad0[0x4c];
    List clients;        // 0x4c
    int unregisterClient(Client *c);
};

// The found path sits after the epilogue in the original (jumping back to LeaveCriticalSection): VC6 /O2 lays out a
// forward-goto target that way; a for+break, while, or inline find/remove all keep that block inline (42-46%).
// MATCH: sound.dll 0x10008920 ?unregisterClient@SoundTimer@m36a_8920@@QAEHPAUClient@2@@Z
int SoundTimer::unregisterClient(Client *c)
{
    void *r;
    EnterCriticalSection(&g_timerLock);
    Node *n;
    for (n = clients.head; n; n = n->next)
        if (n->data == c)
            goto found;
    r = 0;
    goto done;
found:
    r = clients.removeNode(n);
done:
    LeaveCriticalSection(&g_timerLock);
    c->release();
    return r ? 0 : 0x14;
}
}

namespace m36a_89f0 {
extern CRITICAL_SECTION g_timerLock;   // 0x100b4990
struct Flags {
    int v;
    Flags() { v = 0; }
};
struct List {
    void *head;
    void *tail;
    void *cur;
    int count;
    List() { head = 0; tail = 0; cur = 0; count = 0; }
};
struct SoundTimer {
    int delay;           // 0x00
    int m_4, m_8, m_c;
    int m_10;
    int m_14;
    UINT resolution;     // 0x18
    int m_1c;
    int m_20;
    int m_24;
    WAVEFORMATEX wf;     // 0x28
    short pad;
    int flags;           // 0x3c
    int m_40;
    TIMECAPS caps;       // 0x44
    List clients;        // 0x4c
    SoundTimer();
};

// MATCH: sound.dll 0x100089f0 ??0SoundTimer@m36a_89f0@@QAE@XZ
SoundTimer::SoundTimer()
{
    memset(&flags, 0, sizeof(flags));
    timeGetDevCaps(&caps, sizeof(caps));
    resolution = caps.wPeriodMin;
    m_4 = 0;
    m_8 = 0;
    m_c = 0;
    m_14 = 0;
    m_1c = 1;
    m_24 = 0;
    m_40 = 0;
    m_20 = 0;
    wf.wFormatTag = 1;
    wf.cbSize = 0;
    delay = 30;
    wf.nChannels = 2;
    wf.nSamplesPerSec = 44100;
    wf.nAvgBytesPerSec = 176400;
    wf.nBlockAlign = 4;
    wf.wBitsPerSample = 16;
    m_10 = 0x52b;
    memset(&g_timerLock, 0, sizeof(g_timerLock));
    InitializeCriticalSection(&g_timerLock);
}
}

namespace m36a_6330 {
extern int g_mmioOpenCount;   // 0x100b4a08
struct VoiceSlot {
    char pad[0x44];
    HMMIO h;             // 0x44
    ~VoiceSlot();
};
struct Node {
    Node *prev;
    Node *next;
    VoiceSlot *data;
};
struct List {
    Node *head;
    Node *tail;
    int unk8;
    int count;
    inline VoiceSlot *pop()
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
        VoiceSlot *d = n->data;
        delete n;
        count--;
        return d;
    }
};
struct Owner {
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual void v4();
    virtual void v5();
    virtual void v6();
    virtual void v7();
    virtual void v8();
    char pad0[0x44 - 4];
    int state;           // 0x44
    char pad1[0x58 - 0x48];
    int status;          // 0x58
    List slots;          // 0x5c
    int closeMmioNodeList();
};

// MATCH: sound.dll 0x10006330 ?closeMmioNodeList@Owner@m36a_6330@@QAEHXZ
int Owner::closeMmioNodeList()
{
    v8();
    if (state & 2) {
        status |= 8;
        return 0;
    }
    VoiceSlot *p = slots.pop();
    while (p != 0) {
        if (p->h) {
            mmioClose(p->h, 0);
            g_mmioOpenCount--;
        }
        delete p;
        p = slots.pop();
    }
    return 0;
}
}

namespace m36a_8ac0 {
extern int g_timerTickBusy;            // 0x100b49a8
extern CRITICAL_SECTION g_timerLock;   // 0x100b4990
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
    inline void clear()
    {
        void *p = pop();
        while (p != 0)
            p = pop();
    }
};
struct SoundTimer {
    UINT delay;          // 0x00
    int last;            // 0x04
    int elapsed;         // 0x08
    char pad0[0x18 - 0xc];
    UINT resolution;     // 0x18
    char pad1[0x3c - 0x1c];
    int flags;           // 0x3c
    UINT id;             // 0x40
    char pad2[0x4c - 0x44];
    NodeList clients;    // 0x4c
    void stop();
};

// MATCH: sound.dll 0x10008ac0 ?stop@SoundTimer@m36a_8ac0@@QAEXXZ
void SoundTimer::stop()
{
    g_timerTickBusy++;
    if ((flags & 1) && (flags & 2)) {
        g_timerTickBusy++;
        flags &= ~4;
        EnterCriticalSection(&g_timerLock);
        LeaveCriticalSection(&g_timerLock);
        elapsed = 0;
        last = 0;
        timeKillEvent(id);
        timeEndPeriod(resolution);
        flags &= ~1;
    }
    DeleteCriticalSection(&g_timerLock);
    memset(&g_timerLock, 0, sizeof(g_timerLock));
    clients.clear();
}
}

namespace m36a_8ed0 {
extern int g_timerTickBusy;            // 0x100b49a8
extern CRITICAL_SECTION g_timerLock;   // 0x100b4990
struct Client {
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual void v4();
    virtual void v5();
    virtual void v6();
    virtual void v7();
    virtual void v8();
    virtual void v9();
    virtual void v10();
    virtual void v11();
    virtual void v12();
    virtual void v13();
    virtual void tick();                 // +0x38
    virtual void v15();
    virtual void v16();
    virtual void setElapsed(DWORD ms);   // +0x44
};
struct Node {
    Node *prev;
    Node *next;
    Client *data;
};
struct List {
    Node *head;
    Node *tail;
    Node *cur;
    int count;
    inline Client *first()
    {
        cur = head;
        return cur ? cur->data : 0;
    }
    inline Client *next()
    {
        if (cur == 0)
            return 0;
        cur = cur->next;
        return cur ? cur->data : 0;
    }
};
struct SoundTimer {
    DWORD delay;         // 0x00
    DWORD last;          // 0x04
    DWORD elapsed;       // 0x08
    char pad0[0x3c - 0xc];
    int flags;           // 0x3c
    char pad1[0x4c - 0x40];
    List clients;
    static void CALLBACK onTick(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);
};

// The client list is a member object (List at +0x4c): with the fields inlined into SoundTimer the store of last is
// scheduled after the head load (98.9%). first() as cur = head or through a local compiles the same (not determined).
// MATCH: sound.dll 0x10008ed0 ?onTick@SoundTimer@m36a_8ed0@@SGXIIKKK@Z
void CALLBACK SoundTimer::onTick(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
    char buf[256];
    if (g_timerTickBusy)
        return;
    SoundTimer *t = (SoundTimer *)dwUser;
    if (t && !(t->flags & 4))
        return;
    g_timerTickBusy = 1;
    EnterCriticalSection(&g_timerLock);
    if (t && !(t->flags & 4)) {
        g_timerTickBusy++;
        LeaveCriticalSection(&g_timerLock);
        return;
    }
    DWORD now = timeGetTime();
    if (t->last) {
        if (now > t->last)
            t->elapsed = now - t->last;
        else
            t->elapsed = t->last - now;
        if (t->elapsed < t->delay - 7) {
            g_timerTickBusy--;
            LeaveCriticalSection(&g_timerLock);
            return;
        }
    } else {
        t->elapsed = 0;
    }
    if (t->elapsed > t->delay + 20) {
        sprintf(buf, "s_10063d98 %u", t->elapsed);
        OutputDebugStringA(buf);
    }
    t->last = now;
    for (Client *c = t->clients.first(); c; c = t->clients.next()) {
        c->setElapsed(t->elapsed);
        c->tick();
    }
    g_timerTickBusy--;
    LeaveCriticalSection(&g_timerLock);
}
}


#define VSLOTS10(p) virtual void p##0(); virtual void p##1(); virtual void p##2(); virtual void p##3(); \
    virtual void p##4(); virtual void p##5(); virtual void p##6(); virtual void p##7(); virtual void p##8(); \
    virtual void p##9();

namespace m36a_5770 {
struct Chunk {
    char pad[0x18];
    char enabled;        // 0x18
    int kind;            // 0x1c
    int type;            // 0x20
    int id;              // 0x24
};
struct Ctrl {
    char head[0x2c];
    Chunk chunk;         // 0x2c
    void appendIfAccepted(Chunk *c);
};
struct Node {
    Node *prev;
    Node *next;
    Chunk *data;
};
struct Sound {
    VSLOTS10(a) VSLOTS10(b) VSLOTS10(c) VSLOTS10(d) VSLOTS10(e) VSLOTS10(f) VSLOTS10(g)
    virtual Ctrl *makeCtrl(int a, int id);   // slot 70 (+0x118)
    char pad[0xa4c - 4];
    Node *head;          // 0xa4c
    Node *tail;          // 0xa50
    Node *cur;           // 0xa54
    int count;           // 0xa58
    inline Chunk *first()
    {
        cur = head;
        return cur ? cur->data : 0;
    }
    inline Chunk *next()
    {
        if (cur == 0)
            return 0;
        cur = cur->next;
        return cur ? cur->data : 0;
    }
    inline Chunk *prev()
    {
        if (cur == 0)
            return 0;
        cur = cur->prev;
        return cur ? cur->data : 0;
    }
    void processWaveChunks();
};

// MATCH: sound.dll 0x10005770 ?processWaveChunks@Sound@m36a_5770@@QAEXXZ
void Sound::processWaveChunks()
{
    Ctrl *ctrl;
    for (Chunk *c = first(); c; c = next()) {
        switch (c->type) {
        case 0:
            ctrl = makeCtrl(0, c->id);
            ctrl->appendIfAccepted(c);
            break;
        case 3: {
            Chunk *p = prev();
            next();
            if (p->enabled) {
                if (p->kind == 3)
                    ctrl = p ? (Ctrl *)((char *)p - 0x2c) : 0;
                if (ctrl)
                    ctrl->appendIfAccepted(c);
            }
            break;
        }
        }
    }
}
}

namespace m36a_9e40 {
struct SoundTimer {
    void killEvent();
    int unregisterClient(void *c);
};
extern SoundTimer *g_soundTimer;   // 0x100b49ec
struct Sound {
    virtual ~Sound();
    char pad[0xac - 4];
    int flags;           // 0xac
};
struct SNode {
    SNode *prev;
    SNode *next;
    Sound *data;
};
struct SoundList {
    SNode *head;
    SNode *tail;
    SNode *cur;
    int count;
    inline Sound *first()
    {
        cur = head;
        return cur ? cur->data : 0;
    }
    inline Sound *next()
    {
        if (cur == 0)
            return 0;
        cur = cur->next;
        return cur ? cur->data : 0;
    }
    inline void remove(Sound *s)
    {
        SNode *n;
        for (n = head; n; n = n->next)
            if (n->data == s)
                break;
        if (n == 0)
            return;
        if (n->prev)
            n->prev->next = n->next;
        else
            head = n->next;
        SNode *nx = n->next;
        if (nx) {
            nx->prev = n->prev;
            cur = nx;
        } else {
            tail = n->prev;
            cur = 0;
        }
        delete n;
        count--;
    }
};
extern SoundList g_soundList;   // 0x100b5000
struct Seq {
    ~Seq();
};
struct QNode {
    QNode *prev;
    QNode *next;
    Seq *data;
};
struct SeqList {
    QNode *head;
    QNode *tail;
    int unk8;
    int count;
    inline Seq *pop()
    {
        QNode *n = head;
        if (n == 0)
            return 0;
        QNode *nx = n->next;
        if (nx) {
            nx->prev = 0;
            head = nx;
        } else {
            tail = 0;
            head = 0;
        }
        Seq *d = n->data;
        delete n;
        count--;
        return d;
    }
};
extern SeqList g_seqList;   // 0x100b4fe8
struct Midi_Device {
    char pad0[0x2c];
    SoundTimer *timer;   // 0x2c
    char pad1[0x3c - 0x30];
    HMIDIOUT midiOut;    // 0x3c
    void drainCommandQueue();
    int shutdown();
};

// MATCH: sound.dll 0x10009e40 ?shutdown@Midi_Device@m36a_9e40@@QAEHXZ
int Midi_Device::shutdown()
{
    if (timer) {
        timer->killEvent();
        g_soundTimer->unregisterClient(this);
    }
    Sound *s = g_soundList.first();
    drainCommandQueue();
    while (s) {
        s->flags &= ~0x20;
        Sound *d = s;
        g_soundList.remove(s);
        s = g_soundList.next();
        delete d;
    }
    if (midiOut)
        midiOutClose(midiOut);
    Seq *q = g_seqList.pop();
    while (q != 0) {
        delete q;
        q = g_seqList.pop();
    }
    return 0;
}
}

namespace m36a_8470 {
struct Channel {
    void clearBackref();
};
struct Flags {
    unsigned v;
    inline int test(int b) { return (v >> b) & 1; }
    inline bool on(int b) { return (v >> b) & 1; }
    inline void clear(int b) { v &= ~(1 << b); }
};
struct Snd {
    virtual ~Snd();
    virtual void a1();
    virtual void a2();
    virtual void a3();
    virtual void a4();
    virtual void a5();
    virtual void a6();
    virtual void a7();
    virtual void a8();
    virtual void a9();
    virtual void v10();
    virtual void v11();
    virtual void v12();
    virtual void v13();
    virtual void f38();          // +0x38
    virtual void v15();
    virtual void v16();
    virtual void v17();
    virtual void v18();
    virtual void v19();
    virtual void v20();
    virtual void v21();
    virtual void v22();
    virtual int isBusy();        // +0x5c
    virtual void v24();
    virtual Snd *nextSnd();      // +0x64
    virtual void v26();
    virtual void v27();
    virtual void v28();
    virtual void v29();
    virtual int kind();          // +0x78
    virtual void v31();
    virtual void f80();          // +0x80
    char pad0[0x58 - 4];
    Flags f58;                   // 0x58
    char pad1[0x70 - 0x5c];
    Channel ch;                  // 0x70
    char pad2[0xc8 - 0x74];
    Flags fC8;                   // 0xc8
    Flags fCC;                   // 0xcc
    char pad3[0x1424 - 0xd0];
    char flags1424;              // 0x1424
    void rewind();
};
struct SNode {
    SNode *prev;
    SNode *next;
    Snd *data;
};
struct SndList {
    SNode *head;
    SNode *tail;
    SNode *cur;
    int count;
    inline Snd *first()
    {
        cur = head;
        return cur ? cur->data : 0;
    }
    inline Snd *next()
    {
        if (cur == 0)
            return 0;
        cur = cur->next;
        return cur ? cur->data : 0;
    }
    inline void remove(Snd *s)
    {
        SNode *n;
        for (n = head; n; n = n->next)
            if (n->data == s)
                break;
        if (n == 0)
            return;
        if (n->prev)
            n->prev->next = n->next;
        else
            head = n->next;
        SNode *nx = n->next;
        if (nx) {
            nx->prev = n->prev;
            cur = nx;
        } else {
            tail = n->prev;
            cur = 0;
        }
        delete n;
        count--;
    }
};
struct Engine {
    char pad[0x1c8];
    SndList list;                // 0x1c8
    void postNotification(Snd *s, int code);
};
extern Engine *g_engine;   // 0x100b4a20

// The flag words at +0x58/+0xc8/+0xcc are member objects with inline accessors: test() returning int gives
// 'test al, 8' / 'and eax, 1', on() returning bool gives the 'shr; test 1' forms, and clearing through the member
// object keeps VC6 from reusing the cleared +0xc8 value (plain masks / bitfields: 78-85%).
// MATCH: sound.dll 0x10008470 ?flushSoundList@m36a_8470@@YAHXZ
int flushSoundList()
{
    timeGetTime();
    SndList *l = &g_engine->list;
    Snd *o = l->first();
    while (o) {
        Snd *s;
        switch (o->kind()) {
        case 1:
            s = o;
            if (!s->isBusy()) {
                o = l->next();
                l->remove(s);
                if (s->fCC.v & 0x400)
                    s->ch.clearBackref();
                s->fC8.clear(6);
                if (!(s->f58.v & 2) && (!s->f58.on(4) || (s->flags1424 & 1))) {
                    if (!s->fC8.test(0) && !s->fCC.on(7))
                        g_engine->postNotification(s, 5);
                    else if (!s->fC8.test(0) && s->fCC.on(7))
                        delete s;
                    else
                        s->rewind();
                } else {
                    g_engine->postNotification(s, 3);
                }
            } else {
                o = l->next();
            }
            break;
        case 4:
            s = o;
            if (!s->isBusy()) {
                o = o->nextSnd();
                if (s->f58.test(3)) {
                    s->f80();
                    s->f38();
                }
            } else {
                o = o->nextSnd();
            }
            break;
        case 6:
            s = o;
            if (!s->isBusy()) {
                o = o->nextSnd();
                s->f80();
            } else {
                o = o->nextSnd();
            }
            break;
        default:
            o = o->nextSnd();
            break;
        }
    }
    return 0;
}
}

namespace m36a_64e0 {
extern int g_mmioOpenCount;   // 0x100b4a08
struct VoiceSlot {
    char pad0[0x1c];
    int pos;             // 0x1c
    unsigned end;        // 0x20
    char pad1[0x44 - 0x24];
    HMMIO h;             // 0x44
    char pad2[0x9c - 0x48];
    char flags9c;        // 0x9c
    char pad3[0xcc - 0x9d];
    unsigned loops;      // 0xcc
    ~VoiceSlot();
};
typedef int (*RenderFn)(char **cursor, int *pos, int frames, VoiceSlot *v, int flags);
extern RenderFn g_renderFns[];   // 0x1005b4a4
struct Node {
    Node *prev;
    Node *next;
    VoiceSlot *data;
};
struct NodeList {
    Node *head;
    Node *tail;
    Node *cur;
    int count;
    inline VoiceSlot *pop()
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
        VoiceSlot *d = n->data;
        delete n;
        count--;
        return d;
    }
    inline VoiceSlot *first()
    {
        cur = head;
        return cur ? cur->data : 0;
    }
    inline VoiceSlot *next()
    {
        if (cur == 0)
            return 0;
        cur = cur->next;
        return cur ? cur->data : 0;
    }
};
struct Wave_Device {
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual void v4();
    virtual void v5();
    virtual void v6();
    virtual void v7();
    virtual void onBufferDone();     // +0x20
    virtual void v9();
    virtual void v10();
    virtual void v11();
    virtual void v12();
    virtual void v13();
    virtual void v14();
    virtual void v15();
    virtual void v16();
    virtual void v17();
    virtual void v18();
    virtual void v19();
    virtual void v20();
    virtual void v21();
    virtual void v22();
    virtual void v23();
    virtual void v24();
    virtual void v25();
    virtual void v26();
    virtual void v27();
    virtual void v28();
    virtual void v29();
    virtual void v30();
    virtual void v31();
    virtual void onFlushed();        // +0x80
    char pad0[0x58 - 4];
    int state;           // 0x58
    NodeList slots;      // 0x5c
    char pad1[0xc8 - 0x6c];
    int pending;         // 0xc8
    int played;          // 0xcc
    int render(char *dst, int frames, unsigned fmt, int *out);
};

// Tracker name Wave_Device::freeBufferList; the body renders the head buffer through a per-format routine table
// (0x1005b4a4) and frees buffers it finished. The format parameter slot is reused for the play position (address
// passed to the routine), as the original's [esp+0x28] shows.
// MATCH: sound.dll 0x100064e0 ?render@Wave_Device@m36a_64e0@@QAEHPADHIPAH@Z
int Wave_Device::render(char *dst, int frames, unsigned fmt, int *out)
{
    int wide = 0;
    RenderFn fn = g_renderFns[fmt];
    if (fmt >= 12)
        wide = 1;
    if (state & 8) {
        VoiceSlot *p = slots.pop();
        while (p != 0) {
            if (p->h) {
                mmioClose(p->h, 0);
                g_mmioOpenCount--;
            }
            delete p;
            p = slots.pop();
        }
        onFlushed();
    }
    VoiceSlot *v = slots.first();
    if (v == 0) {
        *out = 0;
        if (wide)
            memset(dst, 0, frames * 8);
        return 0x26;
    }
    char *cursor = dst;
    fmt = v->pos;
    int done = fn(&cursor, (int *)&fmt, frames, v, 0);
    *out = done;
    for (unsigned n = v->loops; n > 0; n--) {
        slots.next();
        slots.pop();
        mmioClose(v->h, 0);
        g_mmioOpenCount--;
        delete v;
        v = slots.first();
    }
    if (v) {
        v->pos = fmt;
        if (fmt >= v->end && !(v->flags9c & 1)) {
            played += pending + 1;
            pending = 0;
            state &= ~0x10;
            state |= 4;
            onBufferDone();
            slots.pop();
            mmioClose(v->h, 0);
            g_mmioOpenCount--;
            delete v;
        }
    }
    if (done != frames && wide)
        memset(cursor, 0, (frames - done) * 8);
    return 0;
}
}

namespace m36a_7010 {
struct DsBuffer {
    char pad0[0x18];
    int m_18;
    int m_1c;
    char pad1[0xb4 - 0x20];
    WORD blockAlign;     // 0xb4
    int getPosition(DWORD *bytes, DWORD *offset, DWORD elapsed);
    int restore(DWORD offset, DWORD bytes, void **p1, DWORD *n1, void **p2, DWORD *n2);
    int unlock(void *p1, DWORD n1, void *p2, DWORD n2);
    int isBusy();
};
struct Snd {
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual void v4();
    virtual void v5();
    virtual void v6();
    virtual void v7();
    virtual void v8();
    virtual void v9();
    virtual void v10();
    virtual void v11();
    virtual void v12();
    virtual void v13();
    virtual void v14();
    virtual void v15();
    virtual void v16();
    virtual void v17();
    virtual void v18();
    virtual void v19();
    virtual void v20();
    virtual void v21();
    virtual void v22();
    virtual int isBusy();        // +0x5c
    virtual int isActive();      // +0x60
    virtual void v25();
    virtual void v26();
    virtual void v27();
    virtual void v28();
    virtual void v29();
    virtual int kind();          // +0x78
    virtual void v31();
    virtual void v32();
    virtual void v33();
    virtual void v34();
    virtual void v35();
    virtual void v36();
    virtual void v37();
    virtual void v38();
    virtual void v39();
    virtual void v40();
    virtual void v41();
    virtual void v42();
    virtual void v43();
    virtual void v44();
    virtual void v45();
    virtual void v46();
    virtual void v47();
    virtual void v48();
    virtual void v49();
    virtual void v50();
    virtual int isStreaming();   // +0xcc
};
struct StreamSnd : Snd {
    char pad0[0xbc - 4];
    WAVEFORMATEX wf;             // 0xbc
    void streamWaveSamples(short *mix, DWORD frames, int fmt, int *out);
};
struct CaptureSnd : Snd {
    char pad0[0x58 - 4];
    int flags;                   // 0x58
    char pad1[0xd0 - 0x5c];
    DWORD offset;                // 0xd0
    int pos;                     // 0xd4
    WAVEFORMATEX wf;             // 0xd8
    void computeCaptureDuration();
    int render(void *dst, DWORD frames, int fmt, int *out);
};
struct Channel {
    char pad0[0x54];
    DsBuffer *buf;               // 0xc4
    int flags;                   // 0xc8
    int state;                   // 0xcc
    char pad1[0x180 - 0x60];
    WAVEFORMATEX wf;             // 0x1f0
    char pad2[0x230 - 0x192];
    DWORD lastElapsed;           // 0x2a0
    void renderChannel(short *mix, DWORD frames, int fmt, int *out);
    void serviceBuffer();
    void renderStreamed(DWORD elapsed, int a, int *out);
    void stubReturnB(DWORD elapsed, int a, int *out);
    void setLoopFlag(int i);
};
struct ChanSnd : Snd {
    char pad0[0x70 - 4];
    Channel ch;                  // 0x70
};
struct SNode {
    SNode *prev;
    SNode *next;
    Snd *data;
};
struct SndList {
    SNode *head;
    SNode *tail;
    SNode *cur;
    int count;
    inline Snd *first()
    {
        cur = head;
        return cur ? cur->data : 0;
    }
    inline Snd *next()
    {
        if (cur == 0)
            return 0;
        cur = cur->next;
        return cur ? cur->data : 0;
    }
};
struct Engine {
    char pad[0x1c8];
    SndList list;                // 0x1c8
};
extern Engine *g_engine;         // 0x100b4a20
extern short g_mixBuffer[];      // 0x10069990

struct Device {
    int m_0;
    void *mixDst;                // 0x04
    char pad0[0x18 - 8];
    DWORD lastTime;              // 0x18
    WAVEFORMATEX wf;             // 0x1c
    char pad1[0x30 - 0x2e];
    HMMIO hmmio;                 // 0x30
    int m_34;                    // 0x34
    inline int fmtIndex(WAVEFORMATEX *src, int &first)
    {
        int idx;
        if (src->wBitsPerSample == 8)
            idx = (wf.wBitsPerSample == 16) ? 8 : 4;
        else
            idx = 0;
        if (src->nChannels == 1)
            idx += 2;
        if (wf.nChannels == 1)
            idx++;
        if (first) {
            idx += 12;
            first = 0;
        }
        return idx;
    }
    void flushSoundList();
    void writeWaveChunk(DsBuffer *buf, short *mix, DWORD bytes, DWORD offset);
    int writeWaveFile(DsBuffer *buf);
};

// The frames test compiles the same as a ternary, an if/else assignment or a goto to the return (not determined);/n// separate early returns or a combined || condition move the return-0 block (82-85%). The body sits inside/n// if (g_engine) {...} return 3; so the return-3 block comes last, as in the original.
// MATCH: sound.dll 0x10007010 ?writeWaveFile@Device@m36a_7010@@QAEHPAUDsBuffer@2@@Z
int Device::writeWaveFile(DsBuffer *buf)
{
    int out;
    void *p2;
    DWORD frames;
    DWORD n2;
    DWORD n1;
    void *p1;
    DWORD offset;
    DWORD bytes;
    if (!buf)
        return 0xa;
    DWORD now = timeGetTime();
    if (lastTime == 0)
        lastTime = now;
    int r = buf->getPosition(&bytes, &offset, now - lastTime);
    if (r)
        return r;
    frames = buf->blockAlign ? bytes / buf->blockAlign : 0;
    if (!frames)
        return 0;
    DWORD elapsed = now - lastTime;
    lastTime = now;
    m_34 = 0;
    int first = 1;
    if (g_engine) {
    for (Snd *s = g_engine->list.first(); s; s = g_engine->list.next()) {
        switch (s->kind()) {
        case 1: {
            ChanSnd *c = (ChanSnd *)s;
            if (!c->isActive())
                break;
            if (!(c->ch.state & 4)) {
                if (c->isBusy() && !c->isStreaming()) {
                    c->ch.lastElapsed = elapsed;
                    c->ch.renderChannel(g_mixBuffer, frames, fmtIndex(&c->ch.wf, first), &out);
                }
                if (!(c->ch.state & 4)) {
                    if (c->isBusy() && c->isStreaming())
                        c->ch.serviceBuffer();
                    if (!(c->ch.state & 4)) {
                        if (c->isBusy() && c->isStreaming() && !(c->ch.flags & 1)) {
                            c->ch.renderStreamed(elapsed, 0, &out);
                            c->ch.lastElapsed = elapsed;
                        }
                    }
                }
            }
            if (c->ch.state & 4) {
                if (c->isStreaming()) {
                    c->ch.stubReturnB(elapsed, 0, &out);
                    c->ch.lastElapsed = elapsed;
                }
                if ((c->ch.state & 4) && c->ch.buf && !c->ch.buf->isBusy()) {
                    Channel *ch = &c->ch;
                    ch->flags |= 0x10;
                    for (int i = 0; i < 16; i++)
                        ch->setLoopFlag(i);
                }
            }
            break;
        }
        case 4: {
            CaptureSnd *c = (CaptureSnd *)s;
            if (!c->isBusy())
                break;
            if (!(c->flags & 0x10)) {
                c->offset = offset;
                c->pos = -buf->m_1c;
                c->flags |= 0x10;
            } else {
                c->pos += buf->m_18;
            }
            c->computeCaptureDuration();
            c->render(mixDst, frames, fmtIndex(&c->wf, first), &out);
            break;
        }
        case 6: {
            StreamSnd *st = (StreamSnd *)s;
            if (!st->isBusy())
                break;
            st->streamWaveSamples(g_mixBuffer, frames, fmtIndex(&st->wf, first), &out);
            break;
        }
        }
    }
    if (first) {
        buf->restore(offset, bytes, &p1, &n1, &p2, &n2);
        memset(p1, 0, n1);
        if (p2)
            memset(p2, 0, n2);
        if (hmmio) {
            mmioWrite(hmmio, (char *)p1, n1);
            if (p2)
                mmioWrite(hmmio, (char *)p2, n2);
        }
        buf->unlock(p1, n1, p2, n2);
        flushSoundList();
        return 0;
    }
    writeWaveChunk(buf, g_mixBuffer, bytes, offset);
    flushSoundList();
    return 0;
    }
    return 3;
}
}
