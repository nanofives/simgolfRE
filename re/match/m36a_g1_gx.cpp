// FLAGS sound.dll: /O2 /GX
// m36a (round 36) group g1: sound.dll functions with an EH frame, matched by hand from the original instructions.

#include <windows.h>
#include <string.h>

namespace m36a_3f30 {
struct NodeList {
    void *head;
    void *tail;
    int unk8;
    int count;
    NodeList() { head = 0; tail = 0; unk8 = 0; count = 0; }
    ~NodeList();
};
struct Sound5b6a8 {
    Sound5b6a8();
    virtual ~Sound5b6a8();
    char pad[0x948 - 4];
};
struct Sound : Sound5b6a8 {
    int table[0x40];     // 0x948
    int m_a48;           // 0xa48
    NodeList list;       // 0xa4c
    Sound();
    virtual ~Sound();
    int setSourceType(int t);
};

// MATCH: sound.dll 0x10003f30 ??0Sound@m36a_3f30@@QAE@XZ
Sound::Sound()
{
    memset(table, 0, sizeof(table));
    setSourceType(5);
    m_a48 = 0;
}
}

namespace m36a_92e0 {
void timerCallback();
struct SoundTimer {
    char pad[0x24];
    void (*callback)();  // 0x24
    char pad2[0x5c - 0x28];
    SoundTimer();
};
extern int g_soundInitFlags;      // 0x100b49d8
extern SoundTimer *g_soundTimer;  // 0x100b49ec

// Reads its SECOND stack argument ([esp+0x14] after the 0xc-byte EH frame); the first is unused. With extern "C" VC6
// emits no EH frame for the new-expression here, so the source is C++ linkage.
// MATCH: sound.dll 0x100092e0 ?init_sound_timer@m36a_92e0@@YAHHH@Z
int init_sound_timer(int unused, int flags)
{
    g_soundInitFlags = flags;
    if (!(flags & 1)) {
        g_soundTimer = new SoundTimer;
        g_soundTimer->callback = timerCallback;
    }
    return 0;
}
}

namespace m36a_9cb0 {
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
    ~NodeList()
    {
        void *p = pop();
        while (p != 0)
            p = pop();
    }
};
struct CommandQueue {
    int x;
    void clear();
};
struct DeviceBase {
    virtual ~DeviceBase();
    char pad[0x40 - 4];
};
struct Midi_Device : DeviceBase {
    NodeList list;       // 0x40
    CommandQueue queue;  // 0x50
    virtual ~Midi_Device();
};

// MATCH: sound.dll 0x10009cb0 ??1Midi_Device@m36a_9cb0@@UAE@XZ
Midi_Device::~Midi_Device()
{
    queue.clear();
}
}

namespace m36a_6020 {
struct NodeList {
    void *head;
    void *tail;
    int unk8;
    int count;
    NodeList() { head = 0; tail = 0; unk8 = 0; count = 0; }
    ~NodeList();
};
struct Ch5c178 {
    Ch5c178();
    virtual ~Ch5c178();
    char pad[0x58 - 4];
};
struct WaveStream : Ch5c178 {
    int m_58;            // 0x58
    NodeList list;       // 0x5c
    MMIOINFO info;       // 0x6c
    int m_b4;
    int m_b8;
    int m_bc;
    int m_c0;
    int m_c4;
    int m_c8;
    int m_cc;
    int m_d0;
    int m_d4;
    WAVEFORMATEX wf;     // 0xd8
    WaveStream();
    virtual ~WaveStream();
    int setSourceType(int t);
};

// MATCH: sound.dll 0x10006020 ??0WaveStream@m36a_6020@@QAE@XZ
WaveStream::WaveStream()
{
    m_c8 = 0;
    memset(&m_58, 0, sizeof(m_58));
    m_b8 = 0;
    m_bc = 0;
    m_b4 = 0;
    memset(&info, 0, sizeof(info));
    m_c4 = 0;
    m_c0 = 0;
    setSourceType(4);
    m_d4 = 0;
    m_c0 = 0;
    m_cc = 0;
    m_d0 = 0;
    memset(&wf, 0, sizeof(wf));
}
}

namespace m36a_9a90 {
struct NodeList {
    void *head;
    void *tail;
    int unk8;
    int count;
    NodeList() { head = 0; tail = 0; unk8 = 0; count = 0; }
    ~NodeList();
};
struct CommandQueue {
    int x;
    CommandQueue();
};
struct MidiDeviceBase {
    MidiDeviceBase();
    virtual ~MidiDeviceBase();
    char pad[0x24 - 4];
};
struct Midi_Device : MidiDeviceBase {
    int m_24;
    int m_28;
    int m_2c;
    int m_30;
    int m_34;
    int m_38;
    int m_3c;
    NodeList list;       // 0x40
    CommandQueue queue;  // 0x50
    char pad[0x4058 - 0x54];
    Midi_Device()
    {
        memset(&m_24, 0, sizeof(m_24));
        m_28 = 0;
        m_38 = 0x7f;
        m_2c = 0;
        m_30 = 0xf;
        m_34 = 0;
        m_3c = 0;
    }
    virtual ~Midi_Device();
};
extern Midi_Device *g_midiDevice;   // 0x100b4a1c

// MATCH: sound.dll 0x10009a90 ?create_device@m36a_9a90@@YAHPAPAUMidi_Device@1@@Z
int create_device(Midi_Device **out)
{
    if (g_midiDevice == 0) {
        g_midiDevice = new Midi_Device;
        *out = g_midiDevice;
        return 0;
    }
    return 0xc;
}
}

#define POP_LIST(T) \
    Node *head; \
    Node *tail; \
    int unk8; \
    int count; \
    inline T pop() \
    { \
        Node *n = head; \
        if (n == 0) \
            return 0; \
        Node *nx = n->next; \
        if (nx) { \
            nx->prev = 0; \
            head = nx; \
        } else { \
            tail = 0; \
            head = 0; \
        } \
        T d = n->data; \
        delete n; \
        count--; \
        return d; \
    } \
    inline void clear() \
    { \
        T p = pop(); \
        while (p != 0) \
            p = pop(); \
    }

namespace m36a_4000 {
struct Node {
    Node *prev;
    Node *next;
    void *data;
};
struct NodeList {
    POP_LIST(void *)
    NodeList() { head = 0; tail = 0; unk8 = 0; count = 0; }
    ~NodeList() { clear(); }
};
struct Sound5b6a8 {
    Sound5b6a8();
    virtual ~Sound5b6a8();
    char pad[0x948 - 4];
};
struct Sound : Sound5b6a8 {
    int table[0x40];     // 0x948
    int m_a48;           // 0xa48
    NodeList list;       // 0xa4c
    Sound();
    virtual ~Sound();
};

// MATCH: sound.dll 0x10004000 ??1Sound@m36a_4000@@UAE@XZ
Sound::~Sound()
{
    list.clear();
}
}

namespace m36a_6160 {
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
struct NodeList {
    POP_LIST(VoiceSlot *)
    NodeList() { head = 0; tail = 0; unk8 = 0; count = 0; }
    ~NodeList() { clear(); }
};
struct Ch5c178 {
    Ch5c178();
    virtual ~Ch5c178();
    char pad[0x58 - 4];
};
struct WaveStream : Ch5c178 {
    int m_58;            // 0x58
    NodeList list;       // 0x5c
    WaveStream();
    virtual ~WaveStream();
};

// MATCH: sound.dll 0x10006160 ??1WaveStream@m36a_6160@@UAE@XZ
WaveStream::~WaveStream()
{
    VoiceSlot *p = list.pop();
    while (p != 0) {
        if (p->h) {
            mmioClose(p->h, 0);
            g_mmioOpenCount--;
        }
        delete p;
        p = list.pop();
    }
}
}

namespace m36a_6850 {
extern int g_mmioOpenCount;   // 0x100b4a08
struct Flags {
    unsigned v;
    inline void set(int b) { v |= 1 << b; }
};
struct BFlags {
    unsigned char v;
    inline void set(int b) { v |= 1 << b; }
};
struct VoiceSlot {
    char pad0[0x18];
    int m_18;
    char pad1[0x20 - 0x1c];
    int m_20;
    char pad2[0x90 - 0x24];
    unsigned frames;     // 0x90
    char pad3[0x98 - 0x94];
    int flags98;                 // 0x98
    BFlags f9c;                  // 0x9c
    char pad4[0xac - 0x9d];
    unsigned short blockAlign;   // 0xac
    char pad5[0xb4 - 0xae];
    VoiceSlot *nextSlot; // 0xb4
    char pad6[0xcc - 0xb8];
    int m_cc;            // 0xcc
    int m_d0;
    int m_d4;            // 0xd4
    int m_d8;            // 0xd8
    char pad7[0x110 - 0xdc];
    VoiceSlot();
    void init(MMIOINFO *mi, WAVEFORMATEX *wf);
    inline void linkTo(VoiceSlot *v)
    {
        flags98 |= 1;
        f9c.set(0);
        nextSlot = v;
        m_cc = 0;
    }
};
struct Node {
    Node *prev;
    Node *next;
    VoiceSlot *data;
    Node(VoiceSlot *d) { prev = 0; next = 0; data = d; }
};
struct NodeList {
    Node *head;
    Node *tail;
    Node *cur;
    int count;
    inline VoiceSlot *last()
    {
        cur = tail;
        return cur ? cur->data : 0;
    }
    inline void append(VoiceSlot *v)
    {
        Node *n = new Node(v);
        if (tail) {
            tail->next = n;
            n->prev = tail;
        } else {
            head = n;
        }
        tail = n;
        count++;
    }
};
struct Wave_Device {
    char pad0[0x58];
    int state;           // 0x58
    NodeList slots;      // 0x5c
    MMIOINFO info;       // 0x6c (hmmio at 0xb0)
    char pad1[0xd8 - 0xb4];
    WAVEFORMATEX wf;     // 0xd8
    int appendBuffer(MMIOINFO *mi);
};

// The byte flags at +0x9c are a member object with an inline set(); a plain char field schedules its load before/n// the +0x98 store (98.5%). +0x98 as a plain int or as the same kind of member object compiles the same (not determined).
// MATCH: sound.dll 0x10006850 ?appendBuffer@Wave_Device@m36a_6850@@QAEHPAU_MMIOINFO@@@Z
int Wave_Device::appendBuffer(MMIOINFO *mi)
{
    if (mi != &info)
        return 0xa;
    unsigned len = mi->pchEndRead - mi->pchBuffer;
    if (len == 0) {
        mmioClose(info.hmmio, 0);
        g_mmioOpenCount--;
        info.hmmio = 0;
        return 0x26;
    }
    VoiceSlot *v = new VoiceSlot;
    v->init(mi, &wf);
    VoiceSlot *prev = slots.last();
    slots.append(v);
    info.hmmio = 0;
    v->frames = len / v->blockAlign;
    v->m_d4 = v->m_20;
    v->m_d8 = v->m_18;
    if (prev) {
        prev->linkTo(v);
    }
    if ((state & 4) && !(state & 1)) {
        state &= ~4;
        state |= 3;
    }
    return 0;
}
}
