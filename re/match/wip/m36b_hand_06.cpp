// FLAGS sound.dll: /O2 /GX
// sound.dll hand matches (round 36, batch m36b): the MIDI sequencer object Sound5b6a8 (vtable 0x1005b6a8), built on
// the channel base Ch5be88 (at 0, 0x60 bytes) and MidiFile (at +0x60, 0x1ac bytes). Layouts are the offsets the
// instructions use.
#include <string.h>

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
    void *first() { cur = head; return cur ? cur->val : 0; }
    void *next() { if (!cur) return 0; cur = cur->next; return cur ? cur->val : 0; }
};

struct MidiFileFlags {
    int open : 1; int keep : 1; int b2 : 1; int b3 : 1; int b4 : 1; int b5 : 1; int b6 : 1; int b7 : 1; int b8 : 1;
    int b9 : 1; int loaded : 1; int rest : 21;
};
struct Ring4 { int a, b, c, d; };

struct MidiTrack;                   // a MidiReader (re/match/m36b_hand_05.cpp)
struct MidiTrack {
    char pad[0x2c];
    unsigned char *pos;         // +2c
    unsigned char running;      // +30
    unsigned int flags;         // +34
    unsigned char b38;          // +38
    int pad3c, pad40, pad44;
    int chan;                   // +48
    int advancePos(int n);
};
unsigned int Midi_readVarLen(unsigned char *p, int *len);

struct MidiFile {
    void *hmmio;                // +0
    int m4[0x12];               // +4
    MidiFileFlags flags;        // +4c
    char pad50[0x100];
    Ring4 ring;                 // +150 (+15c ticks per beat)
    char pad160[0x3c];
    PtrListI list;              // +19c
    MidiFile();
    ~MidiFile();
    int closeFile();
    int loadFile(const char *name);
    int loadFromMemory(void *mem);
    int listGetAt(void **out, unsigned int n);
    void markLoaded() { flags.loaded = 1; }
    void clearB3() { flags.b3 = 0; }
    void resetReadState();
    void clearFlag8();
};

struct Notifier { void notifyChannelNotes(); };
struct FlushElem { void flushMessages(); };

struct SoundEngineFlags { unsigned int b0 : 5; unsigned int b5 : 1; unsigned int rest : 26; };
struct SoundEngine { char pad[0x1a4]; SoundEngineFlags flags; };
extern SoundEngine *g_soundEngine;          // 0x100b4a20

struct MidiDevice { char pad[0x3c]; int m3c; int push(void *s, int cmd, int a, int b); };
extern MidiDevice *g_midiDevice;            // 0x100b4a1c

struct SoundThreadState { int post(unsigned int msg, unsigned int wp, long lp); };
extern SoundThreadState g_soundThreadState; // 0x100b5038

extern "C" __declspec(dllimport) int __stdcall PostMessageA(void *hwnd, unsigned int msg, unsigned int wp, long lp);
extern "C" __declspec(dllimport) unsigned long __stdcall timeGetTime(void);

struct Ch5be88 {
    virtual ~Ch5be88();
#define V(n) virtual void v##n();
    V(1) V(2) V(3) V(4) V(5) V(6) V(7) V(8) V(9) V(10) V(11) V(12) V(13) V(14) V(15) V(16) V(17)
    virtual void setActive(int on);     // slot 18 (+0x48)
    V(19) V(20) V(21) V(22)
    virtual int isPlaying();            // slot 23 (+0x5c)
    V(24) V(25) V(26) V(27) V(28) V(29) V(30) V(31) V(32) V(33) V(34) V(35) V(36) V(37) V(38) V(39)
    V(40) V(41) V(42) V(43) V(44)
#undef V
    char pad4[0x40];
    unsigned int flags44;       // +44
    char pad48[0x10];
    unsigned char m58;          // +58
    char pad59[7];
    Ch5be88();
    void setSourceType(int t);
};

struct SoundWindowObj { void pumpWaveMessages(); };
extern SoundWindowObj g_soundWindowObj;     // 0x100b49b8

struct Elem250 { int a, b, c, program; char rest[0x3c]; Elem250(); ~Elem250(); };

struct SoundRef {
    char *name;                 // +0
    char pad[0x20c];
    int refs;                   // +210
    void notifyChannelNotes();
};

struct PtrListG {
    PtrNode *head, *tail, *cur;
    int count;
    void *first() { cur = head; return cur ? cur->val : 0; }
    void *next() { if (!cur) return 0; cur = cur->next; return cur ? cur->val : 0; }
};
extern PtrListG g_cmdTable;         // 0x100b4fe8
extern void *g_soundWindow;         // 0x100b49c0

struct ChanDescS {
    unsigned int index;         // +0
    char pad[0x18];
    char *name;                 // +1c
    int m20;
    char *altName;              // +24
};

struct ChanRecord { char b[0x28]; ChanRecord(); };

struct Track744 : ChanRecord {
    int m28, m2c;
    SoundRef *m30;
    Track744() { m28 = 0; m2c = 0; m30 = 0; }
    ~Track744();
    void setName(const char *n);
    void setAltName(const char *n);
    void mergeChannelDesc(ChanDescS *d);
    int loadChannelDesc(ChanDescS *d);
};

struct MidiTrackRec { char pad[0x38]; unsigned char flags; };

union Sound5b6a8Flags {
    unsigned int all;
    struct { int b0 : 1; int b1 : 1; int b2 : 1; int b3 : 1; int b4 : 1; int b5 : 1; int b6 : 1; int b7 : 1;
             int b8 : 1; int b9 : 1; int rest : 22; } b;
};

struct Sound5b6a8 : Ch5be88, MidiFile {
    int m20c;
    void *hwnd;                 // +210
    Sound5b6a8Flags m214;       // +214
    PtrListI list218;           // +218
    PtrListI list228;           // +228
    int *m238;
    int m23c, m240, m244, m248, m24c;
    Elem250 elems[16];          // +250
    int m710, m714;
    unsigned int tempo;         // +718
    int m71c;
    double pos;                 // +720
    double msPerTick;           // +728
    int m730, m734, m738, m73c, m740;
    Track744 *tracks[0x80];     // +744
    virtual unsigned int trackCount();
    Sound5b6a8();
    ~Sound5b6a8();
    // setTempo 0x10021ee0 as the constructor sees it (inline)
    void setTempoI(unsigned int bpm)
    {
        float b = (float)bpm;
        float d = (float)(unsigned int)ring.d;
        unsigned int old = tempo;
        tempo = bpm;
        msPerTick = 1000.0f / (d * b * (1.0f / 60.0f));
        pos = (unsigned int)(pos * (old / b));
    }
    int setTempo(unsigned int bpm);
    int closeFile();
    int loadFile(const char *name);
    int loadMemoryAndPreRoll(void *mem, int);
    int preRollTrack(unsigned int i);
    int stepTrackMeta(MidiTrack *t);
    int playEvent(MidiTrack *t, int *len);
    int handleMetaEvent(unsigned char *p, MidiTrack *t);
    int noteOn(unsigned char *p, int chan, MidiTrack *t);
    int sendShortMsg(unsigned char *p, int chan, MidiTrack *t);
    int setChannelVolume(unsigned char *p, int chan, MidiTrack *t);
    int postNotify();
    int startPlayback();
    void allNotesOff(char reset);
    int requestStopDrain();
    void setLevel(int v);
    void applyChannelLevel(int v);
    int setupChannel(ChanDescS *d, char load);
    void deactivate() { m740 = 0; m214.b.b3 = 1; setActive(0); }
    bool engineFlag5() { return g_soundEngine->flags.b5 != 0; }
};

// MATCH: sound.dll 0x10021ee0 ?setTempo@Sound5b6a8@@QAEHI@Z
int Sound5b6a8::setTempo(unsigned int bpm)
{
    float b = (float)bpm;
    float d = (float)(unsigned int)ring.d;
    unsigned int old = tempo;
    tempo = bpm;
    msPerTick = 1000.0f / (d * b / 60.0f);
    pos = (unsigned int)(pos * (old / b));
    return 0;
}

// MATCH: sound.dll 0x100204b0 ?closeFile@Sound5b6a8@@QAEHXZ
int Sound5b6a8::closeFile()
{
    int r = 0;
    if (flags.open) {
        r = MidiFile::closeFile();
        flags44 &= ~1;
    }
    return r;
}

// MATCH: sound.dll 0x10020290 ?loadFile@Sound5b6a8@@QAEHPBD@Z
int Sound5b6a8::loadFile(const char *name)
{
    void *track;
    if (m214.all & 0x40)
        g_soundWindowObj.pumpWaveMessages();
    int r = MidiFile::loadFile(name);
    listGetAt(&track, 1);
    if (r == 0) {
        for (unsigned int i = 0; i < trackCount(); i++)
            preRollTrack(i);
        markLoaded();
        m71c = 0;
        pos = 0;
    }
    return r;
}

// MATCH: sound.dll 0x10020360 ?loadMemoryAndPreRoll@Sound5b6a8@@QAEHPAXH@Z
int Sound5b6a8::loadMemoryAndPreRoll(void *mem, int)
{
    MidiTrackRec *track;
    void *first;
    int r = loadFromMemory(mem);
    listGetAt(&first, 1);
    if (r == 0) {
        for (unsigned int i = 0; i < trackCount(); i++)
            preRollTrack(i);
        markLoaded();
        m71c = 0;
        pos = 0;
        if (list218.count == 0) {
            for (unsigned int j = 0; j < trackCount(); j++) {
                if (listGetAt((void **)&track, j) == 0)
                    track->flags |= 8;
            }
        }
    }
    return r;
}

// MATCH: sound.dll 0x100205f0 ?preRollTrack@Sound5b6a8@@QAEHI@Z
int Sound5b6a8::preRollTrack(unsigned int i)
{
    MidiTrack *t;
    int n;
    if (i >= trackCount())
        return 10;
    int r = listGetAt((void **)&t, i);
    if (r == 0x24)
        return r;
    if (t->flags & 2)
        return 0x29;
    memset((char *)t + 0x58, 0, 0x200);
    unsigned char *p;
    while (Midi_readVarLen(p = t->pos, &n) == 0) {
        unsigned char s = p[1] & 0xf0;
        if (s == 0x90 || s == 0xc0)
            break;
        t->advancePos(n);
        n = 0;
        if (playEvent(t, &n) == 0x29)
            return 0x29;
        t->advancePos(n);
    }
    return 0;
}

// MATCH: sound.dll 0x10020720 ?stepTrackMeta@Sound5b6a8@@QAEHPAUMidiTrack@@@Z
int Sound5b6a8::stepTrackMeta(MidiTrack *t)
{
    int n;
    if (!t)
        return 10;
    if (t->flags & 2)
        return 0x29;
    unsigned char *p;
    while (Midi_readVarLen(p = t->pos, &n) == 0) {
        unsigned char s = p[1] & 0xf0;
        if (s == 0x90 || s == 0xc0)
            break;
        t->advancePos(n);
        n = 0;
        if (playEvent(t, &n) == 0x29)
            return 0x29;
        t->advancePos(n);
    }
    return 0;
}

// MATCH: sound.dll 0x10020870 ?playEvent@Sound5b6a8@@QAEHPAUMidiTrack@@PAH@Z
int Sound5b6a8::playEvent(MidiTrack *t, int *len)
{
    int chan;
    if (!t)
        return 10;
    unsigned char *p = t->pos;
    int status = *p;
    if (status & 0x80) {
        if ((status & 0xf0) != 0xf0) {
            t->running = status;
            chan = status & 0x0f;
            status &= 0xf0;
            t->chan = chan;
        }
        *len = 1;
        p++;
    } else {
        status = t->running & 0xf0;
        chan = t->chan;
    }
    switch (status) {
    case 0xff:
        *len += handleMetaEvent(p, t);
        if (*p == 0x2f)
            return 0x29;
        break;
    case 0x90:
        if (p[1] != 0) {
            *len += noteOn(p, chan, t);
            break;
        }
    case 0x80:
        *len += sendShortMsg(p, chan, t);
        break;
    case 0xc0:
        elems[chan].program = *p;
    case 0xe0:
        (*len)++;
        break;
    case 0xb0:
        *len += setChannelVolume(p, chan, t);
        break;
    default:
        return 0xb;
    }
    return 0;
}

// MATCH: sound.dll 0x10020b60 ?postNotify@Sound5b6a8@@QAEHXZ
int Sound5b6a8::postNotify()
{
    if (!flags.b8) {
        m214.b.b4 = 1;
        if (g_soundEngine && engineFlag5()) {
            PostMessageA(hwnd, 0x7ec, 0, (long)this);
            return 0;
        }
        g_soundThreadState.post(0x7ec, 0, (long)this);
    }
    return 0;
}

// MATCH: sound.dll 0x10020be0 ?startPlayback@Sound5b6a8@@QAEHXZ
int Sound5b6a8::startPlayback()
{
    void *t;
    if (!flags.open)
        resetReadState();
    if (isPlaying() != 0 || flags.b2 || flags.b5)
        return 0xe;
    m740 = 0;
    clearFlag8();
    if (m23c != 0) {
        for (int i = 0; i < m240; i++) {
            listGetAt(&t, m238[i]);
            if (t)
                ((MidiTrackRec *)t)->flags |= 4;
        }
    }
    setActive(1);
    m214.b.b2 = 1;
    for (unsigned int j = 0; j < trackCount(); j++)
        preRollTrack(j);
    markLoaded();
    m71c = 0;
    pos = 0;
    m734 = timeGetTime();
    if (g_midiDevice->push(this, 1, 0, 0) == 0)
        clearB3();
    return 0;
}

// MATCH: sound.dll 0x10020d80 ?allNotesOff@Sound5b6a8@@QAEXD@Z
void Sound5b6a8::allNotesOff(char reset)
{
    if (g_midiDevice->m3c) {
        for (FlushElem *e = (FlushElem *)list.first(); e; e = (FlushElem *)list.next())
            e->flushMessages();
    }
    if (!(m58 & 2) || reset) {
        for (int i = 0; i < 0x7f; i++) {
            Track744 *k = tracks[i];
            if (k && g_soundEngine && k->m30)
                k->m30->notifyChannelNotes();
        }
    }
    flags.b2 = 0;
    deactivate();
    flags.b3 = 1;
}

// MATCH: sound.dll 0x10020ee0 ?requestStopDrain@Sound5b6a8@@QAEHXZ
int Sound5b6a8::requestStopDrain()
{
    if (!flags.b2 && isPlaying()) {
        flags.b2 = 1;
        flags.b3 = 1;
        if (g_midiDevice)
            g_midiDevice->push(this, 2, 0, 0);
    }
    return 0;
}

// MATCH: sound.dll 0x1001f4a0 ??0Sound5b6a8@@QAE@XZ
Sound5b6a8::Sound5b6a8()
{
    m214.all = 0;
    m214.b.b9 = 1;
    m710 = 4;
    m714 = 4;
    msPerTick = 0;
    pos = 0;
    hwnd = 0;
    m71c = 0;
    m730 = 0x7f;
    memset(tracks, 0, sizeof(tracks));
    setSourceType(2);
    m238 = 0;
    m23c = 0;
    m240 = 0;
    m248 = 0;
    m24c = 0;
    m738 = -1;
    m73c = 0;
    setTempoI(120);
    m740 = 0;
}

// MATCH: sound.dll 0x1001ff50 ??1Sound5b6a8@@UAE@XZ
Sound5b6a8::~Sound5b6a8()
{
    if (flags.open)
        MidiFile::closeFile();
    for (int i = 0; i < 0x7f; i++) {
        Track744 *t = tracks[i];
        if (t) {
            t->m30 = 0;
            delete t;
            tracks[i] = 0;
        }
    }
    for (void *v = list218.removeHead(); v; v = list218.removeHead())
        delete v;
}

// MATCH: sound.dll 0x10020fe0 ?setupChannel@Sound5b6a8@@QAEHPAUChanDescS@@D@Z
int Sound5b6a8::setupChannel(ChanDescS *d, char load)
{
    hwnd = g_soundWindow;
    char *name = d->name;
    unsigned int idx = d->index;
    if (idx > 0x7f || !name)
        return 10;
    Track744 *t = tracks[idx];
    if (t) {
        t->m30 = 0;
        delete t;
        tracks[idx] = 0;
    }
    t = new Track744;
    if (d->altName) {
        t->setAltName(d->altName);
        for (SoundRef *s = (SoundRef *)g_cmdTable.first(); s; s = (SoundRef *)g_cmdTable.next()) {
            if (strcmp(s->name, d->altName) == 0) {
                t->m30 = s;
                s->refs++;
                break;
            }
        }
        t->mergeChannelDesc(d);
        tracks[idx] = t;
        return 0;
    }
    t->setName(name);
    t->mergeChannelDesc(d);
    if (load) {
        int r = t->loadChannelDesc(d);
        if (r) {
            if (t) {
                t->m30 = 0;
                delete t;
            }
            tracks[idx] = 0;
            return r;
        }
    }
    tracks[idx] = t;
    return 0;
}

// MATCH: sound.dll 0x10020f50 ?setLevel@Sound5b6a8@@QAEXH@Z
void Sound5b6a8::setLevel(int v)
{
    if (v > 0x7f)
        v = 0x7f;
    else if (v < 0)
        v = 0;
    applyChannelLevel(v);
    m730 = v;
}
