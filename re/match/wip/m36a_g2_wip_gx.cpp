// FLAGS sound.dll: /O2 /GX
// m36a (round 36) group g2: near misses that need /GX (WIP lines, never evidence).

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

// 88.5%: every case is in place except case 17: in its first sub-node post the original loads hwnd into ebp before
// the pushes (no push hoisting) and stores the EH state -1 earlier; after it the second post uses dl/edx where this
// build uses cl/ecx, and `delete p` loads the vtable into edx (this build: eax). The helpers need __forceinline:
// with plain inline VC6 stops inlining remove()/append() inside this function (16.1%).
// WIP: sound.dll 0x1000cd80 ?dispatchCommand@Wave_Device@m36a_cd80@@QAEXXZ 88.5%
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
                    post(s, m);
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
