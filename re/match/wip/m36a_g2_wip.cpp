// FLAGS sound.dll: /O2
// m36a (round 36) group g2: near misses (WIP lines, never evidence).

struct Node {
    Node *prev;
    Node *next;
    void *data;
};

namespace m36a_a670 {
struct Voice {
    char pad00[0xac];
    union {
        unsigned flagsAc;
        unsigned char flagsAcLow;
    };
    char padB0[0x210 - 0xb0];
    void *hwnd210;
    unsigned flags214;
    void allNotesOff(char now);
    void postMidiNotification();
    void cmdHandler7();
    void cmdHandler8(int a);
    void cmdHandler9(int a);
    void cmdHandler10(int a, int b);
    void cmdHandler11(int a, int b);
    void cmdHandler12(int a, bool b);
};

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
    void append(void *d);   // 0x10005e30 (appendNodeB)
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
    inline void *remove2(Node *n)
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
            cur = nx;
        }
        void *d = n->data;
        delete n;
        count--;
        return d;
    }
};

struct SoundEngine {
    char pad00[0x1a4];
    unsigned flags1a4;
};

struct SoundThread {
    int postSoundThreadMessage(unsigned msg, unsigned wp, long lp);
};

extern SoundEngine *g_soundEngine;   // 0x100b4a20
extern SoundThread g_soundThread;    // 0x100b5038
extern "C" __declspec(dllimport) int __stdcall PostMessageA(void *hwnd, unsigned msg, unsigned wp, long lp);

struct Midi_Device {
    char pad00[0x40];
    VoiceList voices;        // +0x40
    CmdNode *ring[0x1000];   // +0x50
    unsigned readIdx;        // +0x4050
    unsigned writeIdx;       // +0x4054
    void scanCommandRing(unsigned *r, unsigned *w);
    void dispatchCommand();
};

// 99.6%: one store pair in the found-path remove shared by cases 3 and 4 is in the other order: the original
// stores tail (+0x44) then cur (+0x48); every spelling tried (statement order, cur = nx, cur = n->next, if/else
// swapped) gives cur first, as case 2 has.
// WIP: sound.dll 0x1000a670 ?dispatchCommand@Midi_Device@m36a_a670@@QAEXXZ 99.6%
void Midi_Device::dispatchCommand()
{
    unsigned r = readIdx;
    unsigned w = writeIdx;
    if (r == w)
        return;
    unsigned n = (w - r) & 0xfff;
    if (n)
        scanCommandRing(&r, &w);
    r = (r + 1) & 0xfff;
    for (unsigned k = 0; k < n; k++) {
        CmdNode *c = ring[r];
        if (c) {
            Voice *v;
            Node *nd;
            switch (c->type) {
            case 1:
                v = c->voice;
                if (v && !(v->flagsAcLow & 0x20)) {
                    v->flagsAc |= 0x20;
                    voices.append(v);
                }
                delete c;
                break;
            case 2:
                v = c->voice;
                nd = voices.find(v);
                if (nd)
                    voices.remove(nd);
                v->allNotesOff(0);
                v->flagsAc &= ~0x20;
                v->postMidiNotification();
                delete c;
                break;
            case 3:
                v = c->voice;
                nd = voices.find(v);
                if (nd)
                    voices.remove2(nd);
                v->allNotesOff(0);
                v->flagsAc &= ~0x20;
                v = c->voice;
                if (v) {
                    v->flags214 |= 0x40;
                    if (g_soundEngine && ((unsigned char)(g_soundEngine->flags1a4 >> 5) & 1))
                        PostMessageA(v->hwnd210, 0x7ef, 0, (long)c);
                    else
                        g_soundThread.postSoundThreadMessage(0x7ef, 0, (long)c);
                }
                break;
            case 4:
                v = c->voice;
                if (v) {
                    nd = voices.find(v);
                    if (nd)
                        voices.remove2(nd);
                    v->allNotesOff(0);
                    v->flagsAc &= ~0x20;
                    v = c->voice;
                    if (v) {
                        v->flags214 |= 0x40;
                        if (g_soundEngine && ((unsigned char)(g_soundEngine->flags1a4 >> 5) & 1))
                            PostMessageA(v->hwnd210, 0x7ef, 0, (long)c);
                        else
                            g_soundThread.postSoundThreadMessage(0x7ef, 0, (long)c);
                    }
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
                if (c->voice == 0)
                    break;
                c->voice->cmdHandler11(c->a, c->b);
            case 12:
                if (c->voice)
                    c->voice->cmdHandler12(c->a, c->b != 0);
                break;
            }
            ring[r] = 0;
        }
        r = (r + 1) & 0xfff;
    }
    readIdx = w;
}
}

