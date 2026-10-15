// FLAGS sound.dll: /O2 /GX
// sound.dll hand matches (round 36, batch m36b): small accessors of the MIDI sequencer Sound5b6a8 (track list at
// +0x218, channel table at +0x250) and of its channel record (Track744 / ChanRecord). Layouts are the offsets the
// instructions use; the class shapes repeat re/match/m36b_hand_06.cpp.

struct PtrNode { PtrNode *prev; PtrNode *next; void *val; };

struct PtrListI {
    PtrNode *head, *tail, *cur;
    int count;
    void *first() { cur = head; return cur ? cur->val : 0; }
    void *next() { if (!cur) return 0; cur = cur->next; return cur ? cur->val : 0; }
};

struct MidiFile {
    char pad[0x19c];
    PtrListI list;              // +19c
    int listGetAt(void **out, unsigned int n);
};

struct Ch5be88 {
    virtual ~Ch5be88();
#define V(n) virtual void v##n();
    V(1) V(2) V(3) V(4) V(5) V(6) V(7) V(8) V(9) V(10) V(11) V(12) V(13) V(14) V(15) V(16) V(17) V(18) V(19) V(20)
    V(21) V(22)
    virtual int isPlaying();            // slot 23 (+0x5c)
    V(24) V(25) V(26) V(27) V(28) V(29) V(30) V(31) V(32)
    virtual void selectTrack(int t);    // slot 33 (+0x84)
    V(34) V(35) V(36)
    virtual int playRange(unsigned int lo, unsigned int hi);  // slot 37 (+0x94)
    V(38) V(39) V(40) V(41) V(42) V(43) V(44)
#undef V
    char pad4[0x5c];
};

struct TrackFlags { unsigned char b0 : 1; unsigned char b1 : 1; unsigned char b2 : 1; unsigned char b3 : 1;
                    unsigned char b4 : 1; unsigned char rest : 3; };

struct Counter34 { char pad[0x34]; int n; };

struct SeqTrack {                       // a MidiReader as the sequencer's track list sees it
    char pad[0x38];
    union { TrackFlags flags; unsigned char bits; };    // +38
    char pad39[0x41f];
    Counter34 *m458;                    // +458
    int m45c;                           // +45c
    int m460;                           // +460
    void rewindTracks();
};

struct ListTrack {                      // an entry of the track list at +0x218
    int m0;
    int id;                             // +4
    unsigned int lo, hi;                // +8, +c
    int m10, m14, m18;
    int m1c;                            // +1c
    int m20;                            // +20
    int m24, m28, m2c;
    int m30;                            // +30
    int m34;                            // +34
    void setField8RaisingMax(int v);
};

struct DescFlags { unsigned char b0 : 1; unsigned char b1 : 1; unsigned char rest : 6; };
struct TrackDesc {
    char pad[0x28];
    unsigned int track;                 // +28
    DescFlags flags;                    // +2c
    int m30;                            // +30
};

struct ChannelParams {                  // +0x14 of a channel entry, 0x38 bytes
    int m0, m4, m8, mc, m10, m14;
    short m18;
    int m1c, m20, m24, m28, m2c, m30, m34;
};
struct ChannelEntry { int a, b, c, program, pan; ChannelParams params; };

struct MidiDevice { int push(void *s, int cmd, int a, int b); };
extern MidiDevice *g_midiDevice;        // 0x100b4a1c

struct Sound5b6a8 : Ch5be88, MidiFile {
    char pad20c[0xc];
    PtrListI list218;                   // +218
    char pad228[0x28];
    ChannelEntry elems[16];             // +250
    char pad710[0xc];
    int m71c;                           // +71c
    char pad720[0x18];
    int m738;                           // +738
    virtual unsigned int trackCount();
    void removeFromList(void *v)
    {
        PtrNode *n;
        for (n = list218.head; n; n = n->next)
            if (n->val == v)
                break;
        if (!n)
            return;
        if (n->prev)
            n->prev->next = n->next;
        else
            list218.head = n->next;
        PtrNode *nx = n->next;
        if (nx) {
            nx->prev = n->prev;
            list218.cur = nx;
        } else {
            list218.tail = n->prev;
            list218.cur = 0;
        }
        delete n;
        list218.count--;
    }
    int setRange45c(unsigned int lo, unsigned int hi, int v)
    {
        SeqTrack *t;
        if (hi < lo || hi >= trackCount())
            return 10;
        if (listGetAt((void **)&t, 0))
            t->bits |= 8;
        for (unsigned int i = lo; i <= hi; i++)
            if (listGetAt((void **)&t, i) == 0)
                t->m45c = v;
        return 0;
    }
    int removeTrackById(int id);
    int cmdHandler7();
    int cmdHandler8(int id);
    int cmdHandler9(int id);
    int cmdHandler10(int id, int v);
    int remarkEventRange(ListTrack *e);
    int replayEventRange(unsigned int lo, unsigned int hi);
    int scheduleEvent(SeqTrack *t, int a, int b, int c);
    int emitEvent(unsigned int i);
    ListTrack *findTrackOrZero(int id);
    int findTrackById(int id, int v);
    int cmdHandler11(int id, int v);
    int cmdHandler12(unsigned int i, char flag);
    int applyTrackFlag(TrackDesc *d);
    int requestCmd12(unsigned int i, unsigned int v);
    char getTrackFlagHigh(unsigned int i);
    void *getListElementAt(void *p, int i);
    int copyChannelParams(ChannelParams *d);
};

// MATCH: sound.dll 0x10023000 ?findTrackOrZero@Sound5b6a8@@QAEPAUListTrack@@H@Z
ListTrack *Sound5b6a8::findTrackOrZero(int id)
{
    if (list218.count == 0)
        return 0;
    ListTrack *e;
    for (e = (ListTrack *)list218.first(); e; e = (ListTrack *)list218.next())
        if (e->id == id)
            break;
    return e;
}

// MATCH: sound.dll 0x10023080 ?findTrackById@Sound5b6a8@@QAEHHH@Z
int Sound5b6a8::findTrackById(int id, int v)
{
    if (list218.count == 0)
        return 0x24;
    ListTrack *e;
    for (e = (ListTrack *)list218.first(); e; e = (ListTrack *)list218.next())
        if (e->id == id)
            break;
    if (!e)
        return 0x24;
    e->setField8RaisingMax(v);
    return 0;
}

// MATCH: sound.dll 0x10023c70 ?cmdHandler11@Sound5b6a8@@QAEHHH@Z
int Sound5b6a8::cmdHandler11(int id, int v)
{
    if (list218.count == 0)
        return 0x24;
    ListTrack *e;
    for (e = (ListTrack *)list218.first(); e; e = (ListTrack *)list218.next())
        if (e->id == id)
            break;
    if (!e)
        return 0x24;
    e->m30 = v;
    return 0;
}

// MATCH: sound.dll 0x10023c20 ?cmdHandler12@Sound5b6a8@@QAEHID@Z
int Sound5b6a8::cmdHandler12(unsigned int i, char flag)
{
    SeqTrack *t;
    int r = listGetAt((void **)&t, i);
    if (r == 0)
        t->flags.b4 = flag & 1;
    return r;
}

// MATCH: sound.dll 0x10023e10 ?applyTrackFlag@Sound5b6a8@@QAEHPAUTrackDesc@@@Z
int Sound5b6a8::applyTrackFlag(TrackDesc *d)
{
    SeqTrack *t;
    unsigned int i;
    if (d && (i = d->track) < trackCount()) {
        listGetAt((void **)&t, i);
        t->flags.b4 = d->flags.b1;
        t->m460 = d->m30;
        return 0;
    }
    return 10;
}

// MATCH: sound.dll 0x10023ea0 ?requestCmd12@Sound5b6a8@@QAEHII@Z
int Sound5b6a8::requestCmd12(unsigned int i, unsigned int v)
{
    if (i > trackCount())
        return 10;
    if (g_midiDevice)
        return g_midiDevice->push(this, 0xc, i, v & 0xff);
    return 0x13;
}

// MATCH: sound.dll 0x10023f10 ?getTrackFlagHigh@Sound5b6a8@@QAEDI@Z
char Sound5b6a8::getTrackFlagHigh(unsigned int i)
{
    SeqTrack *t;
    if (i > trackCount())
        return 0;
    listGetAt((void **)&t, i);
    return t->flags.b4;
}

// MATCH: sound.dll 0x100241f0 ?getListElementAt@Sound5b6a8@@QAEPAXPAXH@Z
void *Sound5b6a8::getListElementAt(void *p, int i)
{
    if (!p && i >= 0 && listGetAt(&p, i) == 0)
        return p;
    return 0;
}

// MATCH: sound.dll 0x10022280 ?copyChannelParams@Sound5b6a8@@QAEHPAUChannelParams@@@Z
int Sound5b6a8::copyChannelParams(ChannelParams *d)
{
    if (!d)
        return 10;
    ChannelParams *p = &elems[d->m28].params;
    p->m4 = d->m4;
    p->m8 = d->m8;
    p->mc = d->mc;
    p->m10 = d->m10;
    p->m14 = d->m14;
    p->m18 = d->m18;
    p->m1c = d->m1c;
    p->m20 = d->m20;
    p->m24 = d->m24;
    p->m28 = d->m28;
    p->m2c = d->m2c;
    p->m30 = d->m30;
    p->m34 = d->m34;
    return 0;
}

// MATCH: sound.dll 0x100228d0 ?removeTrackById@Sound5b6a8@@QAEHH@Z
int Sound5b6a8::removeTrackById(int id)
{
    ListTrack *e;
    for (e = (ListTrack *)list218.first(); e; e = (ListTrack *)list218.next())
        if (e->id == id)
            break;
    if (e) {
        removeFromList(e);
        return 0;
    }
    return 0x24;
}

// MATCH: sound.dll 0x100231e0 ?cmdHandler7@Sound5b6a8@@QAEHXZ
int Sound5b6a8::cmdHandler7()
{
    if (list218.count == 0)
        return 0x24;
    ListTrack *e;
    for (e = (ListTrack *)list218.first(); e; e = (ListTrack *)list218.next())
        if (e->id == m738)
            break;
    if (!e)
        return 0x24;
    if (e->m1c)
        selectTrack(e->m1c);
    return playRange(e->lo, e->hi);
}

// MATCH: sound.dll 0x10023540 ?cmdHandler8@Sound5b6a8@@QAEHH@Z
int Sound5b6a8::cmdHandler8(int id)
{
    if (list218.count == 0)
        return 0x24;
    if (m738 == id)
        return 0;
    ListTrack *e;
    for (e = (ListTrack *)list218.first(); e; e = (ListTrack *)list218.next())
        if (e->id == id)
            break;
    if (!e)
        return 0x24;
    if (e->m1c)
        selectTrack(e->m1c);
    return remarkEventRange(e);
}

// MATCH: sound.dll 0x100238b0 ?cmdHandler9@Sound5b6a8@@QAEHH@Z
int Sound5b6a8::cmdHandler9(int id)
{
    if (list218.count == 0)
        return 0x24;
    ListTrack *e;
    for (e = (ListTrack *)list218.first(); e; e = (ListTrack *)list218.next())
        if (e->id == id)
            break;
    if (!e)
        return 0x24;
    e->m34 = 0;
    return replayEventRange(e->lo, e->hi);
}

// MATCH: sound.dll 0x10023a60 ?cmdHandler10@Sound5b6a8@@QAEHHH@Z
int Sound5b6a8::cmdHandler10(int id, int v)
{
    SeqTrack *t;
    if (list218.count == 0)
        return 0x24;
    ListTrack *e;
    for (e = (ListTrack *)list218.first(); e; e = (ListTrack *)list218.next())
        if (e->id == id)
            break;
    if (!e)
        return 0x24;
    return setRange45c(e->lo, e->hi, v);
}

// MATCH: sound.dll 0x10023610 ?remarkEventRange@Sound5b6a8@@QAEHPAUListTrack@@@Z
int Sound5b6a8::remarkEventRange(ListTrack *e)
{
    SeqTrack *t;
    unsigned int hi = e->hi, lo = e->lo;
    if (hi < lo || hi >= trackCount())
        return 10;
    if (listGetAt((void **)&t, 0))
        t->bits |= 8;
    if (isPlaying() == 0) {
        for (unsigned int i = lo; i <= hi; i++)
            if (listGetAt((void **)&t, i) == 0)
                t->bits |= 8;
    } else {
        for (unsigned int j = lo; j <= hi; j++) {
            if (listGetAt((void **)&t, j) == 0 && t) {
                t->bits |= 8;
                scheduleEvent(t, m71c, e->m0, e->m20);
                if (t->m458)
                    t->m458->n++;
            }
        }
    }
    return 0;
}

// MATCH: sound.dll 0x10023950 ?replayEventRange@Sound5b6a8@@QAEHII@Z
int Sound5b6a8::replayEventRange(unsigned int lo, unsigned int hi)
{
    SeqTrack *t;
    if (hi < lo || hi >= trackCount())
        return 10;
    if (listGetAt((void **)&t, 0))
        t->bits |= 8;
    if (isPlaying() == 0) {
        for (unsigned int i = lo; i <= hi; i++)
            if (listGetAt((void **)&t, i) == 0)
                t->bits &= ~8;
    } else {
        for (unsigned int j = lo; j <= hi; j++) {
            if (listGetAt((void **)&t, j) == 0 && (t->bits & 8)) {
                t->bits &= ~8;
                emitEvent(j);
                t->rewindTracks();
            }
        }
    }
    return 0;
}

struct ChanFlags { int b0 : 1; int b1 : 1; int b2 : 1; int b3 : 1; int b4 : 1; int rest : 27; };
struct DescBits { unsigned int b0 : 1; unsigned int b1 : 1; unsigned int b2 : 1; unsigned int b3 : 1;
                  unsigned int b4 : 1; unsigned int rest : 27; };
struct ChanDesc {
    int m0;
    union { ChanFlags f; DescBits u; unsigned int all; } flags;     // +4
    int m8, mc, m10, m14, m18;
    char *name;                                         // +1c
};

struct Track744 {
    int m0;
    union { ChanFlags f; unsigned int all; } flags;     // +4
    int m8, mc, m10, m14;
    void setName(const char *n);
    int loadChannelDesc(ChanDesc *d);
    void mergeChannelDesc(ChanDesc *d);
};

// MATCH: sound.dll 0x10024280 ?loadChannelDesc@Track744@@QAEHPAUChanDesc@@@Z
int Track744::loadChannelDesc(ChanDesc *d)
{
    if (!d)
        return 10;
    m0 = d->m0;
    m8 = d->m8;
    mc = d->mc;
    m10 = d->m10;
    m14 = d->m14;
    flags.f.b0 = d->flags.f.b0;
    flags.f.b1 = d->flags.f.b1;
    setName(d->name);
    return 0xb;
}

// MATCH: sound.dll 0x10024360 ?mergeChannelDesc@Track744@@QAEXPAUChanDesc@@@Z
void Track744::mergeChannelDesc(ChanDesc *d)
{
    if (!d)
        return;
    m0 = d->m0;
    if (d->flags.u.b0) {
        flags.f.b0 = 1;
        m8 = d->m8;
        mc = d->mc;
    }
    if (d->flags.u.b1) {
        flags.f.b1 = 1;
        m10 = d->m10;
        m14 = d->m14;
    }
    if (d->flags.u.b3)
        flags.f.b3 = 1;
    else
        flags.f.b3 = 0;
    if (!d->flags.u.b4)
        flags.f.b4 = 0;
    else
        flags.f.b4 = 1;
    if (d->name)
        setName(d->name);
}
