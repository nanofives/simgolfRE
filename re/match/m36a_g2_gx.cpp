// FLAGS sound.dll: /O2 /GX
#include <string.h>
// m36a (round 36) group g2: sound.dll functions with an EH frame (operator new of an object with a constructor).

namespace m36a_b050 {
struct Wave_Device {
    char pad[0x41e0];
    Wave_Device();
};

extern Wave_Device *g_waveDeviceObj;   // 0x100b4a20

// MATCH: sound.dll 0x1000b050 ?create_device@m36a_b050@@YAHPAPAUWave_Device@1@@Z
int create_device(Wave_Device **out)
{
    if (g_waveDeviceObj == 0) {
        g_waveDeviceObj = new Wave_Device;
        *out = g_waveDeviceObj;
        return 0;
    }
    return 0xc;
}
}

namespace m36a_c2f0 {
// 0x10-byte command node; its constructor 0x100087f0 (listNode3Init) clears the first three dwords.
struct CmdNode {
    int a, b, c, d;
    CmdNode();
};

struct WaveCmdQueue {
    char pad00[0x1d8];
    CmdNode *ring[0x1000];  // +0x1d8
    int readIdx;            // +0x41d8
    int writeIdx;           // +0x41dc
    int pushNode(int b, int a);
};

// MATCH: sound.dll 0x1000c2f0 ?pushNode@WaveCmdQueue@m36a_c2f0@@QAEHHH@Z
int WaveCmdQueue::pushNode(int b, int a)
{
    int i = (writeIdx + 1) & 0xfff;
    if (i == readIdx)
        return 0x22;
    ring[i] = new CmdNode;
    ring[i]->a = a;
    ring[i]->b = b;
    writeIdx = i;
    return 0;
}
}

namespace m36a_dbe0 {
struct CmdNode {
    int a, b, c, d;
    CmdNode();
};

struct SoundObj {
    char pad00[0x1420];
    void *hwnd1420;
    int f1424;
    unsigned char flags1428;
};

struct SoundThread {
    int postSoundThreadMessage(unsigned msg, unsigned wp, long lp);
};
extern SoundThread g_soundThread;   // 0x100b5038

extern "C" __declspec(dllimport) int __stdcall PostMessageA(void *hwnd, unsigned msg, unsigned wp, long lp);

struct Wave_Device {
    char pad00[0x1a4];
    unsigned flags1a4;
    int postNotification(SoundObj *o, int msg);
};

// MATCH: sound.dll 0x1000dbe0 ?postNotification@Wave_Device@m36a_dbe0@@QAEHPAUSoundObj@2@H@Z
int Wave_Device::postNotification(SoundObj *o, int msg)
{
    if (o == 0)
        return 0xa;
    CmdNode *n = new CmdNode;
    n->b = (int)o;
    n->a = msg;
    o->flags1428 |= 1;
    // `shr edx, 5; test dl, 1`: a byte cast of the shifted word; a bit field or `(x >> 5) & 1` folds to `test cl, 0x20`
    if ((unsigned char)(flags1a4 >> 5) & 1)
        PostMessageA(o->hwnd1420, 0x7ee, 0, (long)n);
    else
        g_soundThread.postSoundThreadMessage(0x7ee, 0, (long)n);
    return 0;
}
}

namespace m36a_a240 {
struct CmdNode {
    int a, b, c, d;
    CmdNode();
};

struct CommandQueue {
    char pad00[0x50];
    CmdNode *ring[0x1000];  // +0x50
    int readIdx;            // +0x4050
    int writeIdx;           // +0x4054
    int push(int b, int a, int c, int d);
};

// MATCH: sound.dll 0x1000a240 ?push@CommandQueue@m36a_a240@@QAEHHHHH@Z
int CommandQueue::push(int b, int a, int c, int d)
{
    int i = (writeIdx + 1) & 0xfff;
    if (i == readIdx)
        return 0x22;
    ring[i] = new CmdNode;
    ring[i]->a = a;
    ring[i]->b = b;
    ring[i]->c = c;
    ring[i]->d = d;
    writeIdx = i;
    return 0;
}
}

namespace m36a_b1d0 {
struct Node {
    Node *prev;
    Node *next;
    void *data;
};
struct VoiceBase {
    VoiceBase();
    virtual ~VoiceBase();
    char pad04[0x1a0];
};

struct SoundList {
    Node *head;
    Node *tail;
    Node *cur;
    int count;
    SoundList()
    {
        head = 0;
        tail = 0;
        cur = 0;
        count = 0;
    }
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
    ~SoundList()
    {
        void *p = pop();
        while (p != 0)
            p = pop();
    }
};

struct CommandQueue {
    void *ring[0x1000];
    int readIdx;
    int writeIdx;
    CommandQueue();    // 0x10008860
    ~CommandQueue();   // 0x10008890
};

extern void *g_waveDevice;   // 0x100b49f0
extern void *g_waveStream;   // 0x100b49f4
extern void *g_waveBuffer;   // 0x100b49f8

struct Wave_Device : VoiceBase {
    int flags1a4;
    int f1a8;
    int f1ac;
    int busy1b0;
    int step1b4;
    int volume1b8;
    int f1bc;
    int f1c0;
    int f1c4;
    SoundList sounds;        // +0x1c8
    CommandQueue commands;   // +0x1d8
    Wave_Device();
    virtual ~Wave_Device();
    int stop();
};

// MATCH: sound.dll 0x1000b2e0 ??1Wave_Device@m36a_b1d0@@UAE@XZ
Wave_Device::~Wave_Device()
{
    if (g_waveDevice)
        stop();
}

// MATCH: sound.dll 0x1000b1d0 ??0Wave_Device@m36a_b1d0@@QAE@XZ
Wave_Device::Wave_Device()
{
    g_waveDevice = 0;
    g_waveBuffer = 0;
    g_waveStream = 0;
    f1a8 = 0;
    f1ac = 0;
    busy1b0 = 0;
    volume1b8 = 0x7f;
    memset(&flags1a4, 0, 4);   // a plain `flags1a4 = 0` stores directly; the original's lea + store needs this
    step1b4 = 0;
    f1c4 = 0;
}
}
