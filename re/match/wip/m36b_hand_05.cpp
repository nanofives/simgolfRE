// FLAGS sound.dll: /O2 /GX
// sound.dll hand matches (round 36, batch m36b): the MIDI file / track objects (MidiFile 0x1ac bytes, no vtable;
// MidiReader vtable 0x1005b68c on the owning-list base 0x1005b58c) and small helpers.
#include <string.h>

struct VObj { virtual ~VObj(); };
struct PtrNode { PtrNode *prev; PtrNode *next; void *val; };

struct PtrList {
    PtrNode *head, *tail, *cur;
    int count;
    PtrList() { head = 0; tail = 0; cur = 0; count = 0; }
    ~PtrList();
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

// MATCH: sound.dll 0x1001f1c0 ??1PtrList@@QAE@XZ
PtrList::~PtrList()
{
    for (void *v = removeHead(); v; v = removeHead())
        ;
}

// the same list with the destructor inline (as the owners below see it)
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

struct SoundTimer { unsigned int resolution; };
extern SoundTimer *g_soundTimer;    // 0x100b49ec

struct MidiFileFlags { int open : 1; int keep : 1; int rest : 30; };

struct MidiFile {
    int hmmio;                  // +0
    char pad4[0x48];
    MidiFileFlags flags;        // +4c
    char pad50[0x124];
    unsigned int tickRate;      // +174
    int m178;                   // +178
    unsigned int baseRate;      // +17c
    int transpose;              // +180
    char pad184[0x18];
    PtrListI list;              // +19c
    ~MidiFile();
    int closeFile();
    void resetReadState();
    void scaleTickRate(unsigned int rate);
};

// MATCH: sound.dll 0x1001e2b0 ??1MidiFile@@QAE@XZ
MidiFile::~MidiFile()
{
    if (flags.open)
        closeFile();
}

// MATCH: sound.dll 0x1001e230 ?resetReadState@MidiFile@@QAEXXZ
void MidiFile::resetReadState()
{
    int keep = flags.keep;
    memset(&flags, 0, sizeof(flags));
    flags.keep = keep;
    m178 = baseRate;
    int t = transpose;
    if (t < -64)
        t = -64;
    else if (t > 63)
        t = 63;
    transpose = t;
}

// MATCH: sound.dll 0x1001e630 ?scaleTickRate@MidiFile@@QAEXI@Z
void MidiFile::scaleTickRate(unsigned int rate)
{
    if (g_soundTimer) {
        unsigned int res = g_soundTimer->resolution;
        if (rate > res)
            tickRate = baseRate / (rate / res);
        else
            tickRate = baseRate;
    }
}

extern "C" __declspec(dllimport) unsigned int __stdcall mmioClose(void *h, unsigned int flags);
extern int g_openMmioCount;         // 0x100b4a08

struct Ring4 { int a, b, c, d; };

// the same object seen from the file that holds its constructor and reset (scaleTickRate inline there)
struct MidiFileI {
    void *hmmio;                // +0
    int m4[0x12];               // +4
    MidiFileFlags flags;        // +4c
    char pad50[0x100];
    Ring4 ring;                 // +150 (+15c = 0x78)
    int m160, m164, m168, m16c, m170;
    unsigned int tickRate;      // +174
    int m178;                   // +178
    unsigned int baseRate;      // +17c
    int m180, m184, m188, m18c, m190, m194, m198;
    PtrListI list;              // +19c
    MidiFileI();
    void resetFileState();
    void scaleTickRate(unsigned int rate)
    {
        if (g_soundTimer) {
            unsigned int res = g_soundTimer->resolution;
            if (rate > res)
                tickRate = baseRate / (rate / res);
            else
                tickRate = baseRate;
        }
    }
};

// MATCH: sound.dll 0x1001d380 ??0MidiFileI@@QAE@XZ
MidiFileI::MidiFileI()
{
    hmmio = 0;
    memset(m4, 0, sizeof(m4));
    memset(&flags, 0, sizeof(flags));
    memset(&ring, 0, sizeof(ring));
    ring.d = 0x78;
    m160 = 0;
    m164 = 0;
    m168 = 0;
    m16c = 0;
    m170 = 0;
    m178 = 0x10000;
    baseRate = 0x10000;
    scaleTickRate(1000);
    m180 = 0;
    m184 = 0;
    m188 = 0;
    m18c = 0;
    m190 = 0;
    m198 = 0;
}

// MATCH: sound.dll 0x1001d4a0 ?resetFileState@MidiFileI@@QAEXXZ
void MidiFileI::resetFileState()
{
    if (hmmio) {
        mmioClose(hmmio, 0);
        g_openMmioCount--;
    }
    hmmio = 0;
    memset(m4, 0, sizeof(m4));
    memset(&flags, 0, sizeof(flags));
    m160 = 0;
    m164 = 0;
    m168 = 0;
    m16c = 0;
    m170 = 0;
    m178 = 0x10000;
    baseRate = 0x10000;
    scaleTickRate(1000);
    m180 = 0;
    m184 = 0;
    m188 = 0;
    m18c = 0;
    m190 = 0;
    m198 = 0;
}

struct CtrlElem {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual int map(int v);
    char pad[0x15];
    char enabled;       // +19
    int type;           // +1c
};

struct Ctrl5b58cX {
    virtual char accept(void *p);
    int m4, m8;
    PtrListI list;      // +c
    int m1c, m20, m24, m28;
    ~Ctrl5b58cX();
    void callSlot18();
};

struct MidiReader : Ctrl5b58cX {
    unsigned char *pos;         // +2c
    unsigned char b30;          // +30
    unsigned int flags;         // +34
    unsigned char b38;          // +38
    unsigned char *data;        // +3c
    int m40, m44, m48, m4c, m50;
    int length;                 // +54
    virtual ~MidiReader();
    void rewindTracks();
    int scanTrack();
};

// Midi_readVarLen 0x1001e710 (re/match/m36b_hand_01.cpp), inline here
static inline unsigned int readVarLen(unsigned char *p, int *len)
{
    unsigned int v = 0;
    *len = 0;
    do {
        v = (v << 7) | (*p & 0x7f);
        (*len)++;
    } while (*p++ & 0x80);
    return v;
}

// MATCH: sound.dll 0x1001ef80 ?scanTrack@MidiReader@@QAEHXZ
int MidiReader::scanTrack()
{
    unsigned char *p = data;
    int total = 0;
    unsigned int status = 0;
    int done = 0;
    int n;
    do {
        total += readVarLen(p, &n);
        p += n;
        if (*p & 0x80) {
            status = *p;
            if ((status & 0xf0) != 0xf0)
                status &= 0xf0;
            p++;
        }
        switch (status) {
        case 0xff: {
            if (*p == 0x2f)
                done = 1;
            p++;
            unsigned int len = readVarLen(p, &n);
            p += len + n;
            break;
        }
        case 0xc0:
        case 0xe0:
            p++;
            break;
        case 0x80:
        case 0x90:
        case 0xb0:
            p += 2;
            break;
        default:
            return 0;
        }
    } while (!done);
    length = total;
    return total;
}

// MATCH: sound.dll 0x1001e7d0 ??1MidiReader@@UAE@XZ
MidiReader::~MidiReader()
{
    if (data && (flags & 4))
        delete data;
    data = 0;
}

// MATCH: sound.dll 0x1001e8e0 ?rewindTracks@MidiReader@@QAEXXZ
void MidiReader::rewindTracks()
{
    pos = data;
    m40 = 0;
    m50 = 0;
    flags &= ~2;
    b38 &= ~4;
    m4c = 0;
    m48 = 0x10;
    b30 = 0;
    callSlot18();
    for (CtrlElem *e = (CtrlElem *)list.first(); e; e = (CtrlElem *)list.next()) {
        if (e->type == 5) {
            e->map(0);
            if (e->enabled == 1)
                b38 |= 1;
            else
                b38 &= ~1;
        }
    }
}

// MATCH: sound.dll 0x1001e6e0 ?byteSwap16@@YAGG@Z
unsigned short byteSwap16(unsigned short v)
{
    unsigned short r;
    for (int i = 0; i < 2; i++)
        ((unsigned char *)&r)[i] = ((unsigned char *)&v)[1 - i];
    return r;
}
