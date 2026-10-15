// FLAGS sound.dll: /O2 /GX
// sound.dll hand matches (round 36, batch m36b): the sound channel classes: base 0x1005bb80, SubChannel 0x1005bae0,
// Ch5bb90 0x1005bb90 (a SubChannel with a source type, a second flag word at +0x58 and a start time). Sound objects
// (+0x40) are created by create_sound / freed by delete_sound; playing channels sit on a global list (head
// 0x100b5070, tail 0x100b506c, links +0x48 / +0x4c). Layouts are the offsets the instructions use.
#include <string.h>

struct SoundImpl {
    virtual void v0();
    virtual void setRate(int r);            // +4
#define V(n) virtual void v##n();
    V(2) V(3)
    virtual int load(const char *name);     // +10
    V(5) V(6)
    virtual int start();                    // +1c
    V(8) V(9) V(10) V(11) V(12) V(13) V(14) V(15)
    virtual void setLevel(int v);           // +40
    virtual void setMode(int m);            // +44
    virtual void setActive(int on);         // +48
    V(19) V(20) V(21) V(22)
    virtual int isBusy();                   // +5c
    virtual int isOpen();                   // +60
    V(25) V(26)
    virtual void setFlags(unsigned int f);  // +6c
    virtual unsigned int getFlags();        // +70
    V(29) V(30) V(31) V(32)
    virtual int queueStart();               // +84
    V(34) V(35) V(36) V(37) V(38)
    virtual void setPan(int p);             // +9c
    V(40) V(41) V(42) V(43) V(44) V(45) V(46) V(47) V(48) V(49)
    virtual unsigned int duration();        // +c8
    virtual void setVolume(int v);          // slot 51 (placeholder)
#undef V
};
extern int g_soundSystemReady;              // 0x100b507c
extern "C" __declspec(dllimport) unsigned long __stdcall timeGetTime(void);

struct ChannelFlags {
    int b0 : 1; int active : 1; int b2 : 1; int b3 : 1; int b4 : 1; int b5 : 1; int b6 : 1; int b7 : 1; int b8 : 1;
    int rest : 23;
};

struct SubChannel;
extern SubChannel *g_activeListHead;        // 0x100b5070
extern SubChannel *g_activeListTail;        // 0x100b506c

int create_sound(SoundImpl **out, const char *name, int a);
void delete_sound(SoundImpl *s);

// vtable 0x1005bb80
struct ChannelBase {
    virtual ~ChannelBase() {}
#define V(n) virtual void c##n();
    V(1) V(2) V(3) V(4) V(5) V(6) V(7) V(8) V(9) V(10) V(11) V(12) V(13) V(14) V(15) V(16) V(17) V(18) V(19)
    V(20) V(21)
    virtual int canLoop();                  // slot 22 (+0x58)
    V(23) V(24) V(25) V(26) V(27) V(28) V(29) V(30)
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
    int unlinkFromActiveList();
    void applyChannelLevel(unsigned int v);
    int setName(const char *n);
};

// MATCH: sound.dll 0x1002b260 ?applyChannelLevel@SubChannel@@QAEXI@Z
void SubChannel::applyChannelLevel(unsigned int v)
{
    v &= 0x7f;
    level = v & 0x7f;
    if (sound)
        sound->setLevel(v);
}

// MATCH: sound.dll 0x1002bb10 ?setName@SubChannel@@QAEHPBD@Z
int SubChannel::setName(const char *n)
{
    if (n) {
        if (name)
            delete name;
        name = new char[strlen(n) + 1];
        strcpy(name, n);
        return 0;
    }
    return 10;
}

// MATCH: sound.dll 0x1002ac00 ??1SubChannel@@UAE@XZ
// MATCH: sound.dll 0x1002b090 ?unlinkFromActiveList@SubChannel@@QAEHXZ
int SubChannel::unlinkFromActiveList()
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

struct Ch5bb90 : SubChannel {
    int type;                               // +54
    unsigned int flags2;                    // +58
    int m5c;
    float gain;                             // +60
    unsigned int duration;                  // +64
    unsigned int startTime;                 // +68
    Ch5bb90();
    virtual ~Ch5bb90();
    void setSourceType(int t) throw();
    void markStarted()
    {
        if (!flags.b0) {
            flags.b0 = 1;
            onStarted();
            if (m30)
                sound->setActive(1);
        }
    }
    bool hasFlag10() { return (flags2 & 0x10) != 0; }
    int play(const char *n);
    int start();
    int isBusy();
    int prepare(const char *n);
    int lazyCreateSound();
    int createStartQueue();
    int applyFlags(unsigned int f);
    unsigned int collectFlags();
};

// MATCH: sound.dll 0x1002add0 ?play@Ch5bb90@@QAEHPBD@Z
int Ch5bb90::play(const char *n)
{
    if (!g_soundSystemReady)
        return 1;
    if (!sound) {
        int r = create_sound(&sound, n, 1);
        if (r)
            return r;
    } else if (sound->isOpen()) {
        return 0xf;
    }
    int r = sound->load(n);
    if (r == 0)
        markStarted();
    else
        flags.b0 = 0;
    char *s = new char[strlen(n) + 1];
    strcpy(s, n);
    if (name)
        delete name;
    name = s;
    return r;
}

// MATCH: sound.dll 0x1002b7c0 ?start@Ch5bb90@@QAEHXZ
int Ch5bb90::start()
{
    int r = 0;
    if (sound) {
        r = sound->start();
    } else if (flags2 & 0x10) {
        lazyCreateSound();
        if (sound)
            r = sound->start();
    } else {
        r = 0x14;
    }
    if (flags2 & 0x10) {
        startTime = timeGetTime();
        onStopped();
        sound = 0;
    }
    return r;
}

// MATCH: sound.dll 0x1002baa0 ?isBusy@Ch5bb90@@QAEHXZ
int Ch5bb90::isBusy()
{
    if (!sound && hasFlag10()) {
        if (startTime && duration > timeGetTime() - startTime)
            return 1;
        return 0;
    }
    if (sound)
        return sound->isBusy();
    return 0;
}

// MATCH: sound.dll 0x1002bc60 ?prepare@Ch5bb90@@QAEHPBD@Z
int Ch5bb90::prepare(const char *n)
{
    unsigned int f = 0;
    if (!sound) {
        int r = create_sound(&sound, n, 1);
        if (r)
            return r;
    }
    if (flags2 & 1)
        f = 1;
    if (canLoop())
        f |= 2;
    unsigned int fl = flags2;
    if (fl & 8)
        f |= 0x40;
    if (fl & 0x10)
        f |= 0x80;
    if (fl & 0x40)
        f |= 0x400;
    if (fl & 2)
        f |= 4;
    sound->setFlags(f);
    if (!(flags2 & 2)) {
        int r = play(n);
        if (r) {
            delete_sound(sound);
            sound = 0;
            return r;
        }
        duration = sound->duration();
        sound->setLevel(level);
        sound->setPan(m5c);
        sound->setMode(m8);
    }
    return 0;
}

// MATCH: sound.dll 0x1002bd80 ?lazyCreateSound@Ch5bb90@@QAEHXZ
int Ch5bb90::lazyCreateSound()
{
    unsigned int f = 0;
    char *n = name;
    if (n) {
        if (!sound) {
            int e = create_sound(&sound, n, 1);
            if (e)
                return e;
        }
        if (flags2 & 1)
            f = 1;
        if (canLoop())
            f |= 2;
        unsigned int fl = flags2;
        if (fl & 4)
            f |= 0x11;
        if (fl & 8) {
            f |= 0x40;
            if (!(fl & 0x20))
                f |= 1;
        }
        if (fl & 0x10)
            f |= 0x80;
        if (fl & 0x20)
            f |= 0x100;
        if (fl & 0x40)
            f |= 0x400;
        sound->setFlags(f);
        f = play(n);
        if (f) {
            delete_sound(sound);
            sound = 0;
            return f;
        }
        duration = sound->duration();
    } else {
        f = 8;
    }
    return f;
}

// MATCH: sound.dll 0x1002bee0 ?createStartQueue@Ch5bb90@@QAEHXZ
int Ch5bb90::createStartQueue()
{
    unsigned int f = 0;
    if (name) {
        if (!sound) {
            int e = create_sound(&sound, name, 1);
            if (e)
                return e;
        }
        if (flags2 & 1)
            f = 1;
        if (canLoop())
            f |= 2;
        sound->setFlags(f);
        f = sound->queueStart();
        if (f == 0)
            markStarted();
    } else {
        f = 8;
    }
    return f;
}

// MATCH: sound.dll 0x1002c040 ?applyFlags@Ch5bb90@@QAEHI@Z
int Ch5bb90::applyFlags(unsigned int f)
{
    if (f & 2)
        m30 = 1;
    if (f & 1)
        flags2 |= 1;
    if (f & 4)
        flags2 |= 2;
    if (f & 0x40)
        flags2 |= 8;
    if (f & 0x80)
        flags2 |= 0x10;
    if (!(f & 4) && (f & 0x10))
        flags2 |= 4;
    if (!(f & 4) && (f & 0x100))
        flags2 |= 0x20;
    if (f & 0x400)
        flags2 |= 0x40;
    if (sound)
        sound->setFlags(f);
    return 0;
}

// MATCH: sound.dll 0x1002c0d0 ?collectFlags@Ch5bb90@@QAEIXZ
unsigned int Ch5bb90::collectFlags()
{
    unsigned int f = 0;
    if (sound)
        f = sound->getFlags();
    if (m30)
        f |= 2;
    if (flags2 & 1)
        f |= 1;
    if (flags2 & 8)
        f |= 0x40;
    if (flags2 & 2)
        f |= 4;
    if (flags2 & 4)
        f |= 0x10;
    if (flags2 & 0x10)
        f |= 0x80;
    if (flags2 & 0x20)
        f |= 0x100;
    return f;
}

// MATCH: sound.dll 0x1002b4e0 ??0Ch5bb90@@QAE@XZ
Ch5bb90::Ch5bb90() : type(0)
{
    memset(&flags2, 0, 4);
    flags.b2 = 1;
    setSourceType(1);
    m5c = 0;
    gain = 1.0f;
    m3c = 0x10;
    duration = 0;
    startTime = 0;
}

// MATCH: sound.dll 0x1002b620 ??1Ch5bb90@@UAE@XZ
Ch5bb90::~Ch5bb90()
{
    if (name)
        delete name;
    name = 0;
    unlink();
}

// MATCH: sound.dll 0x1002ad10 ?setSourceType@Ch5bb90@@QAEXH@Z
void Ch5bb90::setSourceType(int t)
{
    switch (t) {
    case 4:
        flags.b4 = 1;
        type = t;
        break;
    case 2:
        flags.b3 = 1;
        type = t;
        break;
    case 1:
        flags.b2 = 1;
        type = t;
        break;
    case 5:
        flags.b3 = 1;
        flags.b5 = 1;
        type = t;
        break;
    case 6:
        flags.b8 = 1;
        type = t;
        break;
    case 7:
        flags.b7 = 1;
        type = t;
        break;
    }
}

