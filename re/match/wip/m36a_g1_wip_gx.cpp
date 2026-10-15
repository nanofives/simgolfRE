// FLAGS sound.dll: /O2 /GX
// m36a group g1 near misses with an EH frame (WIP lines are never read as evidence).

#include <windows.h>
#include <string.h>

namespace m36a_5440 {
struct CtrlObject {
    int m_4;
    int m_8;
    int m_c;
    int m_10;
    int m_14;
    short m_18;
    int kind;            // 0x1c
    int m_20;
    int m_24;
    CtrlObject();
    virtual ~CtrlObject() {}
};
struct FmtObject : CtrlObject {
    FmtObject() {}
    virtual ~FmtObject() {}
};
struct CtrlHead {
    virtual ~CtrlHead();
    int m_4;
    char name8[0x18];    // 0x08
    char name20[0xc];    // 0x20
};
struct Ctrl : CtrlHead, CtrlObject {
    int m_54;
    char name58[0xe4 - 0x58];   // 0x58
    Ctrl();
};
struct Node {
    Node *prev;
    Node *next;
    CtrlObject *data;
    Node(CtrlObject *d) { prev = 0; next = 0; data = d; }
};
struct Sound {
    char pad[0xa48];
    HMMIO h;             // 0xa48
    Node *head;          // 0xa4c
    Node *tail;          // 0xa50
    Node *cur;           // 0xa54
    int count;           // 0xa58
    inline void append(CtrlObject *o)
    {
        Node *n = new Node(o);
        if (tail) {
            tail->next = n;
            n->prev = tail;
        } else {
            head = n;
        }
        tail = n;
        count++;
    }
    void readFmtChunk();
};

// Remaining diff: the two DWORD locals' stack slots are swapped (original: chunk length at esp+0x10, fmt size at
// esp+0xc). A struct {size, len} local gets the order right but moves the new-expression temp below it (92.9%).
// WIP: sound.dll 0x10005440 ?readFmtChunk@Sound@m36a_5440@@QAEXXZ 97.9
void Sound::readFmtChunk()
{
    FmtObject obj;
    DWORD len, size;
    mmioRead(h, (HPSTR)&len, 4);
    mmioRead(h, (HPSTR)&size, 4);
    mmioRead(h, (HPSTR)&obj.m_10, size);
    if (obj.kind == 3) {
        Ctrl *c = new Ctrl;
        CtrlObject *o = c;
        mmioRead(h, (HPSTR)&len, 4);
        mmioRead(h, c->name20, len);
        mmioRead(h, (HPSTR)&len, 4);
        mmioRead(h, c->name8, len);
        mmioRead(h, (HPSTR)&len, 4);
        mmioRead(h, c->name58, len);
        *o = obj;
        append(o);
    }
}
}

namespace m36a_93e0 {
struct MultiSound {
    char pad[0x1450];
    MultiSound(int type);
    void setName(const char *name);
};
struct Sound5b6a8 {
    char pad[0x948];
    Sound5b6a8();
    void assignName(const char *name);
};
struct WaveStream {
    char pad[0xec];
    WaveStream();
};
struct Sound {
    char pad[0xa60];
    Sound();
};
struct Sound5c450 {
    char pad[0xe8];
    Sound5c450();
};
struct StreamReader {
    char pad[0x94];
    StreamReader();
};
struct Ch5c3a4 {
    char pad[0x10c];
    Ch5c3a4();
};
extern int g_multiSoundType;   // 0x100b49c0
extern void *g_midiDevice;     // 0x100b4a1c

// Type 3 and unknown types reach the default (jump table 0x100096e0: 1 MultiSound, 2 Sound5b6a8, 3 default, 4
// WaveStream, 5 Sound, 6 Sound5c450, 7 StreamReader, 8 Ch5c3a4). Remaining diff: every case loads `out` into the
// other of ecx/edx (case 1 ecx in the original, edx here, alternating through case 8); structure, EH states and
// block order are equal. Detection spellings, return-vs-break, typed out pointers did not flip it.
// WIP: sound.dll 0x100093e0 ?create_sound@m36a_93e0@@YAHPAPAXPBDH@Z 83.3
int create_sound(void **out, const char *name, int type)
{
    if (type == 0) {
        if (name == 0)
            type = 1;
        else if (strstr(name, "s_10063dc0"))
            type = 1;
        else if (strstr(name, "s_10063dbc"))
            type = 2;
        else if (strstr(name, "s_10063db8"))
            type = 3;
        else
            type = strstr(name, "s_10063db4") ? 2 : 0;
    }
    switch (type) {
    case 1: {
        MultiSound *m = new MultiSound(g_multiSoundType);
        *out = m;
        m->setName(name);
        break;
    }
    case 2: {
        if (!g_midiDevice)
            return 3;
        Sound5b6a8 *s = new Sound5b6a8;
        *out = s;
        s->assignName(name);
        break;
    }
    case 4:
        *out = new WaveStream;
        break;
    case 5:
        if (!g_midiDevice)
            return 3;
        *out = new Sound;
        break;
    case 6:
        *out = new Sound5c450;
        break;
    case 7:
        *out = new StreamReader;
        break;
    case 8:
        *out = new Ch5c3a4;
        break;
    default:
        return 0xb;
    }
    return 0;
}
}
