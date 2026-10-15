// FLAGS sound.dll: /O2
// m36a (round 36) group g2: sound.dll Midi_Device / Wave_Device functions matched by hand from the original
// instructions.

struct Node {
    Node *prev;
    Node *next;
    void *data;
};

struct DirectSoundDevice {
    void *vtbl;
    char pad04[0x68];
    unsigned char flags6c;
    int getDeviceCaps();
    int setCooperativeLevel(int level);
    int return0Pop4(int a);
    int getNodeByIndex(int index, char *name, int size);
};

struct MmioStream {
    int closeMmioHandle();
};

extern DirectSoundDevice *g_waveDevice;   // 0x100b49f0
extern MmioStream *g_waveStream;          // 0x100b49f4

// MATCH: sound.dll 0x1000e2d0 ?getWaveDeviceInterface@@YAPAXXZ
void *getWaveDeviceInterface()
{
    if (g_waveDevice)
        return g_waveDevice->vtbl;
    return 0;
}

// MATCH: sound.dll 0x1000e230 ?Wave_Device_caps@@YAHXZ
int Wave_Device_caps()
{
    if (g_waveDevice)
        return g_waveDevice->getDeviceCaps();
    return 0;
}

// MATCH: sound.dll 0x1000b170 ?Wave_Device_closeStream@@YAHXZ
int Wave_Device_closeStream()
{
    if (g_waveStream)
        return g_waveStream->closeMmioHandle();
    return 3;
}

// MATCH: sound.dll 0x1000b140 ?Wave_Device_setCooperativeLevel@@YGHH@Z
int __stdcall Wave_Device_setCooperativeLevel(int level)
{
    if (g_waveDevice == 0)
        return 0x13;
    return g_waveDevice->setCooperativeLevel(level);
}

// MATCH: sound.dll 0x1000e310 ?serviceWaveDevice@@YGHH@Z
int __stdcall serviceWaveDevice(int a)
{
    if (g_waveDevice && (g_waveDevice->flags6c & 0x10))
        return g_waveDevice->return0Pop4(a);
    return 0;
}

// MATCH: sound.dll 0x1000e260 ?Wave_Device_deviceName@@YGHHPADH@Z
int __stdcall Wave_Device_deviceName(int index, char *name, int size)
{
    if (g_waveDevice)
        return g_waveDevice->getNodeByIndex(index, name, size);
    *name = 0;
    return 3;
}

struct Voice {
    char pad00[0xac];
    unsigned char flagsAc;
    void update(int t);
};

struct VoiceList {
    Node *head;
    Node *tail;
    Node *cur;
    int count;
    inline Voice *first()
    {
        cur = head;
        if (cur)
            return (Voice *)cur->data;
        return 0;
    }
    inline Voice *next()
    {
        if (cur) {
            cur = cur->next;
            if (cur)
                return (Voice *)cur->data;
        }
        return 0;
    }
};

struct Device {
    char pad00[0x34];
    int time34;
    char pad38[0x8];
    VoiceList voices;   // +0x40
    void dispatchCommand();
    void serviceVoices();
};

// MATCH: sound.dll 0x1000a050 ?serviceVoices@Device@@QAEXXZ
void Device::serviceVoices()
{
    dispatchCommand();
    for (Voice *v = voices.first(); v != 0; v = voices.next()) {
        if (v->flagsAc & 0x20)
            v->update(time34);
    }
    time34 = 0;
}

struct NodeListG {
    Node *head;
    Node *tail;
    Node *cur;
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
    void clearG();
};

// MATCH: sound.dll 0x1000ae80 ?clearG@NodeListG@@QAEXXZ
void NodeListG::clearG()
{
    void *p = pop();
    while (p != 0)
        p = pop();
}

namespace m36a_c230 {
struct DsBuffer {
    char pad00[0x44];
    int delay44;
    void play(int flags);
};

struct DsDevice {
    char pad00[0x6c];
    unsigned char flags6c;
    char pad6d[0x168 - 0x6d];
    Node *buffers;   // +0x168
};

extern DsDevice *g_waveDevice;   // 0x100b49f0
extern int g_waveFile;           // 0x100b49f8
extern int g_waveFileB1;         // 0x100b49fc
extern int g_waveFileB2;         // 0x100b4a00
struct WaveStream {
    int writeWaveFile(int f);
    int writeWaveFileB(int a, int b);
};
extern WaveStream *g_waveStream;  // 0x100b49f4

struct Wave_Device {
    char pad00[0x1b0];
    int busy1b0;
    int step1b4;
    void dispatchCommand();
    void serviceTick();
};

// MATCH: sound.dll 0x1000c230 ?serviceTick@Wave_Device@m36a_c230@@QAEXXZ
void Wave_Device::serviceTick()
{
    dispatchCommand();
    if (g_waveDevice) {
        for (Node *n = g_waveDevice->buffers; n; n = n->next) {
            DsBuffer *b = (DsBuffer *)n->data;
            if (b->delay44) {
                b->delay44 += step1b4;
                if (b->delay44 >= 0) {
                    b->delay44 = 0;
                    b->play(0);
                }
            }
        }
    }
    busy1b0 = 0;
    if (g_waveStream) {
        if ((g_waveDevice->flags6c & 0x10) == 0)
            g_waveStream->writeWaveFile(g_waveFile);
        else
            g_waveStream->writeWaveFileB(g_waveFileB1, g_waveFileB2);
    }
}
}

namespace m36a_cc70 {
struct Deletable {
    virtual ~Deletable();
};

struct CmdNode {
    int type;
    Deletable *obj;
    int c, d;
    ~CmdNode();   // 0x10008840 (pairClear)
};

struct CommandQueue {
    CmdNode *ring[0x1000];
    int readIdx;
    int writeIdx;
    CmdNode *at(int i);
};

struct Wave_Device {
    char pad00[0x1d8];
    CommandQueue commands;   // +0x1d8
    int flushCommandQueue();
};

// The four case labels are separate statements (one `delete c` each, merged by VC6): with `case 1: case 2: case 3:`
// VC6 emits compares instead of the jump table. `delete c->obj` is written without an `if (c->obj)`: with the
// explicit test VC6 caches 0 in ebp instead of hoisting `lea ebp, [this+0x1d8]` (77.8%).
// MATCH: sound.dll 0x1000cc70 ?flushCommandQueue@Wave_Device@m36a_cc70@@QAEHXZ
int Wave_Device::flushCommandQueue()
{
    int r = commands.readIdx;
    int w = commands.writeIdx;
    if (r == w)
        return 0;
    unsigned n = (w - r) & 0xfff;
    int i = (r + 1) & 0xfff;
    for (unsigned k = 0; k < n; k++) {
        CmdNode *c = commands.at(i);
        if (c) {
            switch (c->type) {
            case 1:
                delete c;
                break;
            case 2:
                delete c;
                break;
            case 3:
                delete c;
                break;
            case 4:
                delete c->obj;
                c->obj = 0;
                delete c;
                break;
            }
            commands.ring[i] = 0;
        }
        i = (i + 1) & 0xfff;
    }
    commands.readIdx = w;
    return 0;
}
}

namespace m36a_be00 {
struct Obj5c {
    virtual void v00(); virtual void v04(); virtual void v08(); virtual void v0c();
    virtual void v10(); virtual void v14(); virtual void v18(); virtual void v1c();
    virtual void v20(); virtual void v24(); virtual void v28(); virtual void v2c();
    virtual void v30(); virtual void v34(); virtual void v38(); virtual void v3c();
    virtual void v40(); virtual void v44(); virtual void v48(); virtual void v4c();
    virtual void v50(); virtual void v54(); virtual void v58(); virtual void v5c();
};

struct SoundTimer {
    void killEvent();
};

struct WaveStream {
    void flushSoundList();
};

struct DsBuffer;

struct DirectSoundDevice {
    void removeBuffer(DsBuffer *b);
    void stopAll();
};

extern Obj5c *g_obj4a1c;                // 0x100b4a1c
extern SoundTimer *g_soundTimer;        // 0x100b49ec
extern DirectSoundDevice *g_waveDevice; // 0x100b49f0
extern WaveStream *g_waveStream;        // 0x100b49f4
extern DsBuffer *g_waveBuffer;          // 0x100b49f8
extern DsBuffer *g_waveBufferB1;        // 0x100b49fc
extern DsBuffer *g_waveBufferB2;        // 0x100b4a00

struct Wave_Device {
    virtual void v00(); virtual void v04(); virtual void v08(); virtual void v0c();
    virtual void v10(); virtual void v14(); virtual void v18(); virtual void v1c();
    virtual void v20(); virtual void v24(); virtual void v28(); virtual void v2c();
    virtual void v30(); virtual void v34(); virtual void v38(); virtual void v3c();
    virtual void v40(); virtual void v44(); virtual void v48(); virtual void v4c();
    char pad04[0x1a0];
    int flags1a4;
    int f1a8;
    int f1ac;
    int busy1b0;
    int step1b4;
    int closeOutput();
};

// MATCH: sound.dll 0x1000be00 ?closeOutput@Wave_Device@m36a_be00@@QAEHXZ
int Wave_Device::closeOutput()
{
    if (flags1a4 & 2)
        return 0;
    if (g_obj4a1c)
        g_obj4a1c->v5c();
    v4c();
    g_soundTimer->killEvent();
    g_waveStream->flushSoundList();
    busy1b0 = 0;
    step1b4 = 0;
    if (g_waveDevice) {
        if (g_waveBuffer) {
            g_waveDevice->removeBuffer(g_waveBuffer);
            g_waveBuffer = 0;
        }
        if (g_waveBufferB1) {
            g_waveDevice->removeBuffer(g_waveBufferB1);
            g_waveBufferB1 = 0;
        }
        if (g_waveBufferB2) {
            g_waveDevice->removeBuffer(g_waveBufferB2);
            g_waveBufferB2 = 0;
        }
        g_waveDevice->stopAll();
    }
    flags1a4 |= 2;
    return 0;
}
}

namespace m36a_b960 {
struct SoundTimer {
    void killEvent();
    void unregisterClient(void *client);
};

struct SoundThread {
    void stopSoundThreadFull();
};

struct SoundWindowObj {
    int f0;
    unsigned char flags;
    void unsubclass();
};

struct DsBuffer;

struct DirectSoundDevice {
    char pad00[0x6c];
    unsigned char flags6c;
    void removeBuffer(DsBuffer *b);
    ~DirectSoundDevice();   // 0x10011290 (close)
};

struct WaveStream {
    ~WaveStream();   // 0x10006ec0 (freeDynArray)
};

extern SoundTimer *g_soundTimer;        // 0x100b49ec
extern DirectSoundDevice *g_waveDevice; // 0x100b49f0
extern WaveStream *g_waveStream;        // 0x100b49f4
extern DsBuffer *g_waveBuffer;          // 0x100b49f8
extern DsBuffer *g_waveBufferB1;        // 0x100b49fc
extern DsBuffer *g_waveBufferB2;        // 0x100b4a00
extern SoundWindowObj g_soundWindowObj; // 0x100b49b8
extern SoundThread g_soundThread;       // 0x100b5038

struct Wave_Device {
    char pad00[0x1a4];
    int flags1a4;
    SoundTimer *timer1a8;
    int flushCommandQueue();
    int stop();
};

// MATCH: sound.dll 0x1000b960 ?stop@Wave_Device@m36a_b960@@QAEHXZ
int Wave_Device::stop()
{
    if (timer1a8) {
        timer1a8->killEvent();
        g_soundTimer->unregisterClient(this);
    }
    if (flags1a4 & 0x20)
        g_soundWindowObj.flags = (g_soundWindowObj.flags & 0xfe) ^ 1;   // `and al, 0xfe; xor al, 1`; `|= 1` gives `or byte ptr`
    else
        g_soundThread.stopSoundThreadFull();
    flushCommandQueue();
    if (g_waveDevice) {
        if (g_waveBuffer) {
            g_waveDevice->removeBuffer(g_waveBuffer);
            g_waveBuffer = 0;
        }
        if (g_waveDevice->flags6c & 8) {
            g_waveDevice->removeBuffer(g_waveBufferB1);
            g_waveDevice->removeBuffer(g_waveBufferB2);
        }
        delete g_waveDevice;
        g_waveDevice = 0;
    }
    if (g_waveStream) {
        delete g_waveStream;
        g_waveStream = 0;
    }
    if (flags1a4 & 0x20)
        g_soundWindowObj.unsubclass();
    return 0;
}
}

namespace m36a_c020 {
struct SoundTimer {
    void killEvent();
    void unregisterClient(void *client);
    void registerClient(void *client);
    void start();
};

struct DsBuffer {
    char pad00[0x38];
    unsigned bytes38;
    char pad3c[0xb4 - 0x3c];
    unsigned short blockAlignB4;
    void setRegionMs(int ms);
    void lockEntire();
    void play(int flags);
};

struct DirectSoundDevice {
    char pad00[0x10];
    unsigned char flags10;
    char pad11[0x148 - 0x11];
    unsigned count148;
    void removeBuffer(DsBuffer *b);
    void releaseAllBuffers();
    void releaseAll();
    int seekListToIndex(unsigned index);
    int create(int hwnd, int flags);
    int createBuffer(DsBuffer **out, int flags, int size);
};

struct WaveStream {
    void resizeDynArray(unsigned n);
};

extern SoundTimer *g_soundTimer;        // 0x100b49ec
extern DirectSoundDevice *g_waveDevice; // 0x100b49f0
extern WaveStream *g_waveStream;        // 0x100b49f4
extern DsBuffer *g_waveBuffer;          // 0x100b49f8
extern int g_hwnd4a3c;                  // 0x100b4a3c

struct Wave_Device {
    virtual void v00(); virtual void v04(); virtual void v08(); virtual void v0c();
    virtual void v10(); virtual void v14(); virtual void v18(); virtual void v1c();
    virtual void v20(); virtual void v24(); virtual void v28(); virtual void v2c();
    virtual void v30(); virtual void v34(); virtual void v38();
    virtual int v3c(int a);
    virtual int v40(int a);
    char pad04[0x1a4];
    SoundTimer *timer1a8;
    int restart(unsigned index);
};

// MATCH: sound.dll 0x1000c020 ?restart@Wave_Device@m36a_c020@@QAEHI@Z
int Wave_Device::restart(unsigned index)
{
    if (g_waveDevice == 0)
        return 3;
    if (index >= g_waveDevice->count148)
        return 0xa;
    if (timer1a8) {
        g_soundTimer->unregisterClient(this);
        g_soundTimer->killEvent();
    }
    if (g_waveBuffer) {
        g_waveDevice->removeBuffer(g_waveBuffer);
        g_waveBuffer = 0;
    }
    g_waveDevice->releaseAllBuffers();
    g_waveDevice->releaseAll();
    if (g_waveDevice->seekListToIndex(index))
        return 6;
    int r = g_waveDevice->create(g_hwnd4a3c, 0);
    if (r)
        return r;
    int size = 0x10000;
    if (g_waveDevice->flags10 & 0x20)
        size = 0x20000;
    if (g_waveDevice->createBuffer(&g_waveBuffer, 0x10088, size) == 0) {
        v3c(0xf);
        g_waveBuffer->setRegionMs(v40(0xe));
        g_waveBuffer->lockEntire();
        g_waveBuffer->play(1);
        unsigned frames = g_waveBuffer->blockAlignB4 ? g_waveBuffer->bytes38 / g_waveBuffer->blockAlignB4 : 0;
        g_waveStream->resizeDynArray(frames * 2);
        timer1a8 = g_soundTimer;
        g_soundTimer->registerClient(this);
        g_soundTimer->start();
        return 0;
    }
    g_waveDevice->releaseAll();
    g_waveBuffer = 0;
    return 0x1e;
}
}

namespace m36a_a330 {
#define V4(n) virtual void v##n##0(); virtual void v##n##4(); virtual void v##n##8(); virtual void v##n##c();
struct Voice {
    virtual ~Voice();                                          // slot 0
    virtual void v04(); virtual void v08(); virtual void v0c();
    V4(1) V4(2) V4(3) V4(4) V4(5) V4(6) V4(7) V4(8) V4(9) V4(a)
    virtual void vb0(); virtual void vb4();
    virtual void stopB8();                                     // +0xb8
    char pad04[0xa8];
    unsigned flagsAc;
    void allNotesOff(char now);
    void closeFile();
    void cmdHandler7();
    void cmdHandler8(int a);
    void cmdHandler9(int a);
    void cmdHandler10(int a, int b);
    void cmdHandler11(int a, int b);
};
#undef V4

struct CmdNode {
    int type;
    Voice *voice;
    int a, b;
    ~CmdNode();   // 0x10008840 (pairClear)
};

struct VoiceList {
    Node *head;
    Node *tail;
    Node *cur;
    int count;
    inline Node *find(void *d)
    {
        Node *n;
        for (n = head; n; n = n->next)
            if (n->data == d)
                break;
        return n;
    }
    inline void *remove(Node *n)
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

struct Midi_Device {
    char pad00[0x40];
    VoiceList voices;        // +0x40
    CmdNode *ring[0x1000];   // +0x50
    unsigned readIdx;        // +0x4050
    unsigned writeIdx;       // +0x4054
    void scanCommandRing(unsigned *r, unsigned *w);
    int drainCommandQueue();
};

// MATCH: sound.dll 0x1000a330 ?drainCommandQueue@Midi_Device@m36a_a330@@QAEHXZ
int Midi_Device::drainCommandQueue()
{
    unsigned r = readIdx;
    unsigned w = writeIdx;
    if (r == w)
        return 0;
    unsigned n = (w - r) & 0xfff;
    if (n)
        scanCommandRing(&r, &w);
    r = (r + 1) & 0xfff;
    for (unsigned k = 0; k < n; k++) {
        CmdNode *c = ring[r];
        if (c) {
            Voice *v;
            Node *n;
            switch (c->type) {
            case 1:
                delete c;
                break;
            case 2:
                v = c->voice;
                v->allNotesOff(0);
                v->flagsAc &= ~0x20;
                v->stopB8();
                delete c;
                break;
            case 3:
                v = c->voice;
                n = voices.find(v);
                if (n)
                    voices.remove(n);
                v->allNotesOff(0);
                v->flagsAc &= ~0x20;
                v->closeFile();
                delete c;
                break;
            case 4:
                v = c->voice;
                if (v) {
                    if (v->flagsAc & 0x20) {
                        n = voices.find(v);
                if (n)
                    voices.remove(n);
                        v->flagsAc &= ~0x20;
                    }
                    delete v;
                    delete c;
                }
                break;
            case 7:
                if (c->voice) {
                    c->voice->cmdHandler7();
                    delete c;
                }
                break;
            case 8:
                v = c->voice;
                if (v) {
                    v->cmdHandler9(c->a);
                    v->cmdHandler8(c->a);
                    delete c;
                }
                break;
            case 9:
                if (c->voice) {
                    c->voice->cmdHandler9(c->a);
                    delete c;
                }
                break;
            case 10:
                if (c->voice)
                    c->voice->cmdHandler10(c->a, c->b);
                break;
            case 11:
                if (c->voice)
                    c->voice->cmdHandler11(c->a, c->b);
                break;
            }
            ring[r] = 0;
        }
        r = (r + 1) & 0xfff;
    }
    readIdx = w;
    return 0;
}
}

namespace m36a_dcf0 {
struct SoundObj {
    char pad00[0xc8];
    unsigned char flagsC8;
};

struct CmdNode {
    int type;
    SoundObj *obj;
    int a, b;
    ~CmdNode();   // 0x10008840 (pairClear)
};

struct CommandQueue {
    CmdNode *ring[0x1000];
    int readIdx;
    int writeIdx;
    CmdNode *at(unsigned i);
};

struct Wave_Device {
    char pad00[0x1d8];
    CommandQueue commands;   // +0x1d8
    int scanCommandRing(unsigned *r, unsigned *w);
};

// MATCH: sound.dll 0x1000dcf0 ?scanCommandRing@Wave_Device@m36a_dcf0@@QAEHPAI0@Z
int Wave_Device::scanCommandRing(unsigned *r, unsigned *w)
{
    unsigned i, j, next;
    for (i = *r, next = j = (i + 1) & 0xfff; i != *w; i = next, next = j = (i + 1) & 0xfff) {
        if (j == *w)
            return 0;
        for (; commands.at(i) && j != *w; j = (j + 1) & 0xfff) {
            if (commands.at(j)) {
                if (commands.at(i)->obj == commands.at(j)->obj) {
                    if (commands.at(i)->type == commands.at(j)->type) {
                        delete commands.at(j);
                        commands.ring[j] = 0;
                    } else if (commands.at(i)->type == 1 && commands.at(j)->type == 2) {
                        SoundObj *o = commands.at(i)->obj;
                        if (o && (o->flagsC8 & 0x40)) {
                            delete commands.at(i);
                            commands.ring[i] = 0;
                        } else {
                            delete commands.at(i);
                            commands.ring[i] = 0;
                            delete commands.at(j);
                            commands.ring[j] = 0;
                        }
                    } else if (commands.at(i)->type == 2 && commands.at(j)->type == 1) {
                        SoundObj *o = commands.at(i)->obj;
                        if (o && !(o->flagsC8 & 0x40)) {
                            delete commands.at(i);
                            commands.ring[i] = 0;
                        } else {
                            delete commands.at(i);
                            commands.ring[i] = 0;
                            delete commands.at(j);
                            commands.ring[j] = 0;
                        }
                    }
                }
            }
        }
    }
    return 0;
}
}

namespace m36a_aaf0 {
struct Voice {
    char pad00[0xac];
    unsigned char flagsAc;
};

struct CmdNode {
    int type;
    Voice *voice;
    int a, b;
    ~CmdNode();   // 0x10008840 (pairClear)
};

struct CommandQueue {
    CmdNode *ring[0x1000];
    int readIdx;
    int writeIdx;
    CmdNode *at(unsigned i);
};

struct Midi_Device {
    char pad00[0x50];
    CommandQueue commands;   // +0x50
    int scanCommandRing(unsigned *r, unsigned *w);
};

// MATCH: sound.dll 0x1000aaf0 ?scanCommandRing@Midi_Device@m36a_aaf0@@QAEHPAI0@Z
int Midi_Device::scanCommandRing(unsigned *r, unsigned *w)
{
    unsigned i, j, next;
    for (i = *r, next = j = (i + 1) & 0xfff; i != *w; i = next, next = j = (i + 1) & 0xfff) {
        if (j == *w)
            return 0;
        for (; commands.at(i) && j != *w; j = (j + 1) & 0xfff) {
            if (commands.at(j)) {
                if (commands.at(i)->voice == commands.at(j)->voice) {
                    if (commands.at(i)->type == commands.at(j)->type && commands.at(i)->a == commands.at(j)->a
                        && commands.at(i)->b == commands.at(j)->b) {
                        delete commands.at(j);
                        commands.ring[j] = 0;
                    } else if (commands.at(i)->type == 1 && commands.at(j)->type == 2) {
                        Voice *v = commands.at(i)->voice;
                        if (v && (v->flagsAc & 0x20)) {
                            delete commands.at(i);
                            commands.ring[i] = 0;
                        } else {
                            delete commands.at(i);
                            commands.ring[i] = 0;
                            delete commands.at(j);
                            commands.ring[j] = 0;
                        }
                    } else if (commands.at(i)->type == 2 && commands.at(j)->type == 1) {
                        Voice *v = commands.at(i)->voice;
                        if (v && !(v->flagsAc & 0x20)) {
                            delete commands.at(i);
                            commands.ring[i] = 0;
                        } else {
                            delete commands.at(i);
                            commands.ring[i] = 0;
                            delete commands.at(j);
                            commands.ring[j] = 0;
                        }
                    }
                }
            }
        }
    }
    return 0;
}
}

namespace m36a_e050 {
struct SoundObj {
    char pad00[0xc8];
    int flagsC8;
};

struct SoundList {
    Node *head;
    Node *tail;
    Node *cur;
    int count;
    inline Node *find(void *d)
    {
        Node *n;
        for (n = head; n; n = n->next)
            if (n->data == d)
                break;
        return n;
    }
    inline void *remove(Node *n)
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

struct Wave_Device {
    char pad00[0x1c8];
    SoundList sounds;   // +0x1c8
    int removeSoundNode(SoundObj *s);
};

// MATCH: sound.dll 0x1000e050 ?removeSoundNode@Wave_Device@m36a_e050@@QAEHPAUSoundObj@2@@Z
int Wave_Device::removeSoundNode(SoundObj *s)
{
    Node *n = sounds.find(s);
    if (n == 0 || sounds.remove(n) == 0)
        return 0x24;
    s->flagsC8 &= ~0x40;
    return 0;
}
}
