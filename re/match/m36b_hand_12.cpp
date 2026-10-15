// FLAGS sound.dll: /O2 /GX
// sound.dll hand matches (round 36, batch m36b): the channel classes built on SubChannel (0x1005bae0): Mixer
// (0x1005bdd8), the voice array on Ch5bb90 (0x1005bcb4), MixBuffer (0x1005be84), Ch5be88, Ch5c178, the "Voice Rx" /
// "Voice Tx" channels (0x1005c234 / 0x1005c2ec), Ch5c3a4 (a Mixer with a stream slot) and Sound5c450. The base
// classes repeat re/match/m36b_hand_11.cpp. Layouts are the offsets the instructions use.
#include <string.h>

extern "C" __declspec(dllimport) unsigned int __stdcall mmioClose(void *h, unsigned int flags);

struct SoundImpl {
    virtual ~SoundImpl();
    virtual void setRate(int r);            // +4
#define V(n) virtual void v##n();
    V(2) V(3)
    virtual int load(const char *name);     // +10
    virtual int stop();                     // +14
    V(6) V(7) V(8) V(9) V(10) V(11) V(12) V(13) V(14) V(15) V(16) V(17) V(18) V(19) V(20) V(21) V(22) V(23)
    V(24) V(25) V(26)
    virtual void setFlags(unsigned int f);  // +6c
    V(28) V(29) V(30) V(31) V(32)
    virtual int queue(int a, int b, int c, int d);   // +84
#undef V
};
struct SoundImpl3 {                         // the same slot 33 with three arguments
#define V(n) virtual void v##n();
    V(0) V(1) V(2) V(3) V(4) V(5) V(6) V(7) V(8) V(9) V(10) V(11) V(12) V(13) V(14) V(15) V(16) V(17) V(18) V(19)
    V(20) V(21) V(22) V(23) V(24) V(25) V(26) V(27) V(28) V(29) V(30) V(31) V(32)
#undef V
    virtual int queue(int a, int b, int c);
};

struct ChannelFlags {
    int b0 : 1; int active : 1; int b2 : 1; int b3 : 1; int b4 : 1; int b5 : 1; int b6 : 1; int b7 : 1; int b8 : 1;
    int rest : 23;
};

struct SubChannel;
extern SubChannel *g_activeListHead;        // 0x100b5070
extern SubChannel *g_activeListTail;        // 0x100b506c

int create_sound(SoundImpl **out, const char *name, int a);
void delete_sound(SoundImpl *s);

struct ChannelBase {
    virtual ~ChannelBase() {}
#define V(n) virtual void c##n();
    V(1) V(2) V(3) V(4)
    virtual void reset();                   // slot 5 (+0x14)
    V(6) V(7) V(8) V(9) V(10) V(11) V(12) V(13) V(14) V(15) V(16) V(17) V(18) V(19) V(20) V(21) V(22)
    virtual int isBusy();                   // slot 23 (+0x5c)
    V(24) V(25) V(26) V(27) V(28) V(29) V(30)
    virtual void onStarted();               // slot 31 (+0x7c)
    virtual void onStopped();               // slot 32 (+0x80)
#undef V
    int level;                              // +4 (0x7f)
    int m8;
    int mc[9];                              // +c
    int m30, m34;
    int rate;                               // +38
    int m3c;
    ChannelBase()
    {
        level = 0x7f;
        m8 = 0;
        memset(mc, 0, sizeof(mc));
        m30 = 0;
    }
};

struct SubChannel : ChannelBase {
    SoundImpl *sound;                       // +40
    ChannelFlags flags;                     // +44
    SubChannel *prev;                       // +48
    SubChannel *next;                       // +4c
    char *name;                             // +50
    SubChannel()
    {
        prev = 0;
        next = 0;
        sound = 0;
        name = 0;
        memset(&flags, 0, sizeof(flags));
        flags.b0 = 0;
        setRate(1000);
    }
    virtual ~SubChannel()
    {
        if (name) {
            delete name;
            name = 0;
        }
        if (sound) {
            delete_sound(sound);
            sound = 0;
        }
        unlink();
    }
    void setRate(int r)
    {
        rate = r;
        if (sound)
            sound->setRate(r);
    }
    int unlink()
    {
        if (!flags.active)
            return 0;
        if (prev)
            prev->next = next;
        else
            g_activeListHead = next;
        if (next)
            next->prev = prev;
        else
            g_activeListTail = prev;
        next = 0;
        flags.active = 0;
        prev = 0;
        return 0;
    }
    int play(const char *n);
    int assignName(const char *n);
    int releaseSound();
};

// MATCH: sound.dll 0x1002ddf0 ?assignName@SubChannel@@QAEHPBD@Z
int SubChannel::assignName(const char *n)
{
    if (n) {
        if (name)
            delete name;
        name = 0;
        name = new char[strlen(n) + 1];
        strcpy(name, n);
        return 0;
    }
    return 10;
}

// MATCH: sound.dll 0x1002dff0 ?releaseSound@SubChannel@@QAEHXZ
int SubChannel::releaseSound()
{
    int r = 0;
    if (sound) {
        if (isBusy() == 0) {
            sound->stop();
            delete_sound(sound);
        } else {
            r = sound->stop();
        }
        sound = 0;
    }
    onStopped();
    flags.b0 = 0;
    return r;
}

// ---- Ch5bb90 as an inline base (its own out-of-line copies match in m36b_hand_11.cpp)
struct Ch5bb90I : SubChannel {
    int type;                               // +54
    unsigned int flags2;                    // +58
    int m5c;
    float gain;                             // +60
    unsigned int duration;                  // +64
    unsigned int startTime;                 // +68
    virtual ~Ch5bb90I()
    {
        if (name)
            delete name;
        name = 0;
        unlink();
    }
};

struct MixerVoiceArray : Ch5bb90I {
    void *voices[0x28];                     // +6c
    virtual ~MixerVoiceArray();
};

// MATCH: sound.dll 0x1002c5e0 ??1MixerVoiceArray@@UAE@XZ
MixerVoiceArray::~MixerVoiceArray()
{
    for (int i = 0; i < 0x28; i++) {
        if (voices[i]) {
            delete voices[i];
            voices[i] = 0;
        }
    }
}

// ---- Mixer
struct Mixer : SubChannel {
    int type;                               // +54
    void *voices[0x28];                     // +58
    Mixer();
    virtual ~Mixer();
    void setSourceType(int t) throw();
};

// NEAR 98.1% (see re/match/wip/m36b_hand_12.cpp): sound.dll 0x1002c9f0 ??0Mixer@@QAE@XZ
Mixer::Mixer() : type(0)
{
    memset(voices, 0, sizeof(voices));
    setSourceType(8);
}

// MATCH: sound.dll 0x1002cb00 ??1Mixer@@UAE@XZ
Mixer::~Mixer()
{
    for (int i = 0; i < 0x28; i++) {
        if (voices[i]) {
            delete voices[i];
            voices[i] = 0;
        }
    }
}

// ---- MixBuffer
struct MixBufFlags { int b0 : 1; int b1 : 1; int b2 : 1; int rest : 29; };
struct MixBuffer {
    virtual void v0();
    int buf[0x80];                          // +4
    char *name;                             // +204
    MixBufFlags flags;                      // +208
    int m20c, m210, m214, m218;
    MixBuffer();
    void setName(const char *n);
};

// MATCH: sound.dll 0x1002cee0 ??0MixBuffer@@QAE@XZ
MixBuffer::MixBuffer()
{
    memset(&flags, 0, sizeof(flags));
    flags.b2 = 0;
    flags.b1 = 1;
    m20c = 0;
    m210 = 0;
    name = 0;
    m214 = 0;
    m218 = 0xfa;
    memset(buf, 0, sizeof(buf));
}

// MATCH: sound.dll 0x1002d240 ?setName@MixBuffer@@QAEXPBD@Z
void MixBuffer::setName(const char *n)
{
    if (name)
        delete name;
    name = new char[strlen(n) + 1];
    strcpy(name, n);
}

// ---- Ch5be88
struct ByteFlags { unsigned char b0 : 1; unsigned char b1 : 1; unsigned char rest : 6; };
struct Ch5be88 : SubChannel {
    int type;                               // +54
    ByteFlags f58;                          // +58
    int m5c;                                // +5c
    Ch5be88();
    virtual ~Ch5be88();
    int open2(const char *n);
};
struct Ch5be88Peer : SubChannel { int type; ByteFlags f58; int m5c; };

// MATCH: sound.dll 0x1002d450 ??0Ch5be88@@QAE@XZ
Ch5be88::Ch5be88() : type(0)
{
    memset(&f58, 0, 1);
    f58.b0 = 1;
    m5c = 0;
    flags.b3 = 1;
}

// MATCH: sound.dll 0x1002d560 ??1Ch5be88@@UAE@XZ
Ch5be88::~Ch5be88()
{
    if (name)
        delete name;
    name = 0;
    if (sound)
        delete_sound(sound);
    sound = 0;
    m5c = 0;
}

// the same body in a second class (Ch5be88_dtorB stores the same vtable 0x1005be88)
struct Ch5be88B : SubChannel {
    int type;
    ByteFlags f58;
    int m5c;
    virtual ~Ch5be88B();
};

// MATCH: sound.dll 0x1002e660 ??1Ch5be88B@@UAE@XZ
Ch5be88B::~Ch5be88B()
{
    if (name)
        delete name;
    name = 0;
    if (sound)
        delete_sound(sound);
    sound = 0;
    m5c = 0;
}

// MATCH: sound.dll 0x1002deb0 ?open2@Ch5be88@@QAEHPBD@Z
int Ch5be88::open2(const char *n)
{
    int r;
    if (!sound)
        r = create_sound(&sound, n, 2);
    if (r)
        return r;
    if ((r = play(n)) != 0)
        return r;
    if (f58.b1) {
        ((Ch5be88Peer *)sound)->f58.b1 = 1;
        int v = m5c;
        if (v)
            ((Ch5be88Peer *)sound)->m5c = v;
    }
    return 0;
}

// ---- NodeB000
struct PtrList4 { int a, b, c, d; PtrList4() { a = 0; b = 0; c = 0; d = 0; } };
struct NodeB000 {
    int m0;
    PtrList4 list;                          // +4
    int m14, m18, m1c, m20;
    NodeB000();
};

// MATCH: sound.dll 0x1002e3f0 ??0NodeB000@@QAE@XZ
NodeB000::NodeB000()
{
    m0 = 0;
    m14 = 0;
    m18 = 0;
    m1c = 0;
    m20 = -1;
}

// ---- Ch5c178
struct Ch5c178 : SubChannel {
    int type;
    Ch5c178();
    virtual ~Ch5c178();
    void setSourceType(int t) throw();
    int open4(int a, int b, int c, int d);
};

// MATCH: sound.dll 0x1002e930 ??0Ch5c178@@QAE@XZ
Ch5c178::Ch5c178() : type(0)
{
    setSourceType(4);
}

// MATCH: sound.dll 0x1002ea30 ??1Ch5c178@@UAE@XZ
Ch5c178::~Ch5c178()
{
    if (sound) {
        delete sound;
        sound = 0;
    }
}

// MATCH: sound.dll 0x1002eb40 ?open4@Ch5c178@@QAEHHHHH@Z
int Ch5c178::open4(int a, int b, int c, int d)
{
    int r = 0;
    if (!sound) {
        r = create_sound(&sound, 0, 4);
        if (r)
            return r;
    }
    if (!sound)
        return r;
    r = sound->queue(a, b, c, d);
    if (r) {
        delete_sound(sound);
        sound = 0;
    }
    onStarted();
    return r;
}

// ---- Voice Rx
struct Ch5c234 : SubChannel {
    int type;
    Ch5c234();
    virtual ~Ch5c234();
    void setSourceType(int t);
    int open6(int a, int b, int c);
};

// MATCH: sound.dll 0x1002ed50 ??0Ch5c234@@QAE@XZ
Ch5c234::Ch5c234() : type(0)
{
    char tmp[] = "Voice Rx";
    setSourceType(6);
    name = new char[sizeof(tmp) + 1];
    strcpy(name, tmp);
}

// MATCH: sound.dll 0x1002ef40 ??1Ch5c234@@UAE@XZ
Ch5c234::~Ch5c234()
{
    if (sound) {
        delete sound;
        sound = 0;
    }
    if (name) {
        delete name;
        name = 0;
    }
}

// MATCH: sound.dll 0x1002f060 ?open6@Ch5c234@@QAEHHHH@Z
int Ch5c234::open6(int a, int b, int c)
{
    int r = 0;
    if (!sound) {
        r = create_sound(&sound, 0, 6);
        if (r)
            return r;
    }
    if (!sound)
        return r;
    r = ((SoundImpl3 *)sound)->queue(a, b, c);
    if (r) {
        delete_sound(sound);
        sound = 0;
    }
    onStarted();
    return r;
}

// ---- Voice Tx
struct Ch5c2ec : SubChannel {
    int type;
    int m58;
    Ch5c2ec();
    virtual ~Ch5c2ec();
    void setSourceType(int t);
    int open7(int a, int b, int c);
};

// NEAR 97.4% (see re/match/wip/m36b_hand_12.cpp): sound.dll 0x1002f220 ??0Ch5c2ec@@QAE@XZ
Ch5c2ec::Ch5c2ec() : type(0)
{
    memset(&m58, 0, 4);
    char tmp[] = "Voice Tx";
    name = new char[sizeof(tmp) + 1];
    strcpy(name, tmp);
    setSourceType(7);
}

// MATCH: sound.dll 0x1002f3f0 ??1Ch5c2ec@@UAE@XZ
Ch5c2ec::~Ch5c2ec()
{
    if (sound)
        delete_sound(sound);
    sound = 0;
    if (name) {
        delete name;
        name = 0;
    }
}

// MATCH: sound.dll 0x1002f5e0 ?open7@Ch5c2ec@@QAEHHHH@Z
int Ch5c2ec::open7(int a, int b, int c)
{
    int r = 0;
    reset();
    if (!sound) {
        r = create_sound(&sound, 0, 7);
        if (r)
            return r;
    }
    if (!sound)
        return r;
    r = ((SoundImpl3 *)sound)->queue(a, b, c);
    if (r != 0 && r != 0x10) {
        delete_sound(sound);
        sound = 0;
    }
    if (sound)
        onStarted();
    return r;
}

// ---- Ch5c3a4: a Mixer with a stream slot
struct StreamChan { void setStreamSource(void *slot, void *owner); void prime(); };
struct MultiSound {
    virtual ~MultiSound();
#define V(n) virtual void v##n();
    V(1) V(2) V(3)
    virtual int load(void *src);            // +10
    V(5) V(6) V(7) V(8) V(9) V(10) V(11) V(12) V(13) V(14) V(15) V(16) V(17) V(18) V(19) V(20) V(21) V(22)
    V(23) V(24) V(25) V(26)
    virtual void setFlags(unsigned int f);  // +6c
#undef V
    char pad4[0x6c];
    StreamChan chan;                        // +70
    char pad71[0x5b];
    unsigned char mcc;                      // +cc
    char pad[0x1450 - 0xcd];
    MultiSound();
};
struct StreamOwner { char pad[0x70]; StreamChan chan; char pad2[0x13a3]; void *m1414; };
struct Cmd { Cmd(int code, void *obj); int m0, m4, m8, mc; };
struct SoundEngine { void pushValue(Cmd *c); };
extern SoundEngine *g_soundEngine;          // 0x100b4a20

struct Ch5c3a4 : Mixer {
    StreamOwner *mf8;                       // +f8
    MultiSound *mfc;                        // +fc
    int m100, m104, m108;
    Ch5c3a4();
    void setSourceType(int t);
    int setSlotSource(unsigned int i);
    void enqueueE(int a, int b, int c);
    int enqueue10();
};

// MATCH: sound.dll 0x1002f740 ??0Ch5c3a4@@QAE@XZ
Ch5c3a4::Ch5c3a4()
{
    mf8 = 0;
    mfc = 0;
    m100 = 0;
    m104 = 0;
    m108 = 0x7f;
    setSourceType(8);
}

// NEAR 99.0% (see re/match/wip/m36b_hand_12.cpp): sound.dll 0x1002f8d0 ?setSlotSource@Ch5c3a4@@QAEHI@Z
int Ch5c3a4::setSlotSource(unsigned int i)
{
    MultiSound **slot = &mfc;
    if (*slot)
        return 0xf;
    if (i >= 0x28)
        return 10;
    if (!voices[i])
        return 0x24;
    unsigned int fl = 0x102;
    if (mf8)
        fl = 0x302;
    MultiSound *m = new MultiSound;
    m->setFlags(fl);
    int r = m->load(voices[i]);
    if (r) {
        if (m)
            delete m;
    } else {
        if (m->mcc & 4)
            m->chan.prime();
        StreamOwner *o = mf8;
        *slot = m;
        if (o)
            o->chan.setStreamSource(slot, &mf8);
    }
    return r;
}

// MATCH: sound.dll 0x1002fb70 ?enqueueE@Ch5c3a4@@QAEXHHH@Z
void Ch5c3a4::enqueueE(int a, int b, int c)
{
    if (g_soundEngine) {
        Cmd *p = new Cmd(0xe, this);
        p->m8 = a;
        p->mc = c;
        g_soundEngine->pushValue(p);
    }
}

// MATCH: sound.dll 0x1002fc10 ?enqueue10@Ch5c3a4@@QAEHXZ
int Ch5c3a4::enqueue10()
{
    if (!g_soundEngine)
        return 1;
    if (mf8)
        mf8->m1414 = &mf8;
    g_soundEngine->pushValue(new Cmd(0x10, this));
    return 0;
}

// ---- Sound5c450: a Voice Rx channel with a list of open streams
struct PtrNode { PtrNode *prev; PtrNode *next; void *val; };
struct PtrListI {
    PtrNode *head, *tail, *cur;
    int count;
    PtrListI() { head = 0; tail = 0; cur = 0; count = 0; }
    ~PtrListI() { for (void *v = removeHead(); v; v = removeHead()) ; }
    void *removeHead()
    {
        PtrNode *n = head;
        if (!n)
            return 0;
        PtrNode *nx = n->next;
        if (nx) {
            nx->prev = 0;
            head = nx;
        } else {
            tail = 0;
            head = 0;
        }
        void *v = n->val;
        delete n;
        count--;
        return v;
    }
    void remove(void *v)
    {
        PtrNode *n;
        for (n = head; n; n = n->next)
            if (n->val == v)
                break;
        if (!n)
            return;
        if (n->prev)
            n->prev->next = n->next;
        else
            head = n->next;
        PtrNode *nx = n->next;
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
extern PtrListI g_openSoundList;            // 0x100b49c8
extern int g_openMmioCount;                 // 0x100b4a08

struct OpenStream;
struct OpenStream {
    char pad[0x44];
    void *hmmio;
    ~OpenStream();
    void close();
};
inline void OpenStream::close()
{
    mmioClose(hmmio, 0);
    g_openSoundList.remove(this);
}

struct Sound5c450 : Ch5c234 {
    int m58;                                // +58
    PtrListI streams;                       // +5c
    int m6c[0x12];                          // +6c
    int mb4, mb8;
    int pad[6];
    int md4, md8, mdc, me0, me4;
    Sound5c450();
    virtual ~Sound5c450();
};

// MATCH: sound.dll 0x10030080 ??0Sound5c450@@QAE@XZ
Sound5c450::Sound5c450()
{
    mb8 = 0;
    mb4 = 0;
    memset(&m58, 0, 4);
    memset(m6c, 0, sizeof(m6c));
    setSourceType(6);
    md4 = 0;
    me4 = 0;
    md8 = 0;
    me0 = 0;
}

// MATCH: sound.dll 0x10030180 ??1Sound5c450@@UAE@XZ
Sound5c450::~Sound5c450()
{
    for (OpenStream *s = (OpenStream *)streams.removeHead(); s; s = (OpenStream *)streams.removeHead()) {
        if (s->hmmio)
            s->close();
        g_openMmioCount--;
        delete s;
    }
}
