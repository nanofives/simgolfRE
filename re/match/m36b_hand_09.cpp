// FLAGS sound.dll: /O2 /GX
// sound.dll hand matches (round 36, batch m36b): the sequencer's event scheduling and channel-command dispatch
// (Sound5b6a8), and the NodeList used for its event queues. The class shapes repeat re/match/m36b_hand_07.cpp; here
// applyTrackFlag and copyChannelParams are inline, as dispatchChannelCmd sees them (their out-of-line copies 0x10023e10
// and 0x10022280 match in m36b_hand_07.cpp).

struct PtrNode {
    PtrNode *prev; PtrNode *next; void *val;
    PtrNode(void *v) { prev = 0; next = 0; val = v; }
};

struct NodeList {
    PtrNode *head, *tail, *cur;
    int count;
    ~NodeList();
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
    void pushBack(void *v)
    {
        PtrNode *n = new PtrNode(v);
        if (tail) {
            tail->next = n;
            n->prev = tail;
            tail = n;
        } else {
            head = n;
            tail = n;
        }
        count++;
    }
    void pushFront(void *v)
    {
        PtrNode *n = new PtrNode(v);
        if (head) {
            head->prev = n;
            n->next = head;
            head = n;
        } else {
            head = n;
            tail = n;
        }
        count++;
    }
    void insert(void *v);
};

// MATCH: sound.dll 0x10024d40 ??1NodeList@@QAE@XZ
NodeList::~NodeList()
{
    for (void *v = removeHead(); v; v = removeHead())
        ;
}

// MATCH: sound.dll 0x10024e60 ?insert@NodeList@@QAEXPAX@Z
void NodeList::insert(void *v)
{
    if (!head) {
        pushBack(v);
        return;
    }
    if (!cur) {
        pushFront(v);
        return;
    }
    PtrNode *p = cur->next;
    if (!p) {
        pushBack(v);
        return;
    }
    PtrNode *n = new PtrNode(v);
    cur->next = n;
    p->prev = n;
    n->prev = cur;
    n->next = p;
    count++;
}

struct PtrListI {
    PtrNode *head, *tail, *cur;
    int count;
};

struct Ring4 { int a, b, c, d; };

struct MidiFile {
    char pad[0x150];
    Ring4 ring;                         // +150 (+15c ticks per beat)
    char pad160[0x3c];
    PtrListI list;                      // +19c
    int listGetAt(void **out, unsigned int n);
};

struct Ch5be88 {
    virtual ~Ch5be88();
#define V(n) virtual void v##n();
    V(1) V(2) V(3) V(4) V(5) V(6) V(7) V(8) V(9) V(10) V(11) V(12) V(13) V(14) V(15) V(16) V(17) V(18) V(19) V(20)
    V(21) V(22) V(23) V(24) V(25) V(26) V(27) V(28) V(29) V(30) V(31) V(32) V(33) V(34) V(35) V(36) V(37) V(38) V(39)
    V(40) V(41) V(42) V(43) V(44)
#undef V
    char pad4[0x5c];
};

struct TrackFlags { unsigned char b0 : 1; unsigned char b1 : 1; unsigned char b2 : 1; unsigned char b3 : 1;
                    unsigned char b4 : 1; unsigned char rest : 3; };

struct VoiceStop { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4();
                   virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9();
                   virtual void stop(); };
struct VoiceGroup : VoiceStop {
    char pad[0x1434];
    int count;                          // +1438
    int pad143c;
    VoiceStop *subs[1];                 // +1440
};

union ShortMsg { unsigned char b[4]; unsigned long dw; };

struct SeqTrack {                       // a MidiReader as the sequencer's track list sees it
    char pad[0x38];
    union { TrackFlags flags; unsigned char bits; };    // +38
    char pad39[0x17];
    unsigned int m50;                   // +50
    unsigned int m54;                   // +54
    VoiceGroup *voices[0x80];           // +58
    ShortMsg msgs[0x80];                // +258
    int m458, m45c;
    int m460;                           // +460
    int appendIfAccepted(void *d);      // Ctrl::appendIfAccepted 0x1001b9a0
};

struct DescFlags { unsigned char b0 : 1; unsigned char b1 : 1; unsigned char rest : 6; };
struct ChannelParams {                  // +0x14 of a channel entry, 0x38 bytes
    int m0, m4, m8, mc, m10, m14;
    short m18;
    int m1c, m20, m24, m28, m2c, m30, m34;
};
struct ChannelEntry { int a, b, c, program, pan; ChannelParams params; };

struct CmdDesc {                        // the channel command block both inline helpers read
    int m0, m4, m8, mc, m10, m14;
    short m18;
    int type;                           // +1c
    int m20, m24;
    unsigned int track;                 // +28
    DescFlags flags;                    // +2c
    int m30;                            // +30
    int m34;
};

struct MidiDevice { char pad[0x3c]; void *hmo; };
extern MidiDevice *g_midiDevice;        // 0x100b4a1c
extern "C" __declspec(dllimport) unsigned int __stdcall midiOutShortMsg(void *hmo, unsigned long msg);
extern int g_trackRoundRobin;           // 0x100b5010

struct Sound5b6a8 : Ch5be88, MidiFile {
    char pad20c[0x2c];
    int *m238;                          // +238
    int m23c;                           // +23c
    char pad240[0x10];
    ChannelEntry elems[16];             // +250
    char pad710[0xc];
    int m71c;                           // +71c
    virtual unsigned int trackCount();
    int applyTrackFlag(CmdDesc *d)
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
    int copyChannelParams(CmdDesc *e)
    {
        ChannelParams *d = (ChannelParams *)e;
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
    int updateChannel(CmdDesc *d);
    int dispatchChannelCmd(CmdDesc *d, int cmd, int idx);
    void scheduleEvent(SeqTrack *t, unsigned int tick, unsigned char mode, int ref);
    int seekTrackToTick(SeqTrack *t, unsigned int tick);
    int nextTrack(SeqTrack **t)
    {
        if (++g_trackRoundRobin >= m23c)
            g_trackRoundRobin = 0;
        return listGetAt((void **)t, m238[g_trackRoundRobin]);
    }
    void allocTrackSlot();
    int emitEvent(unsigned int i);
};

// MATCH: sound.dll 0x10024040 ?dispatchChannelCmd@Sound5b6a8@@QAEHPAUCmdDesc@@HH@Z
int Sound5b6a8::dispatchChannelCmd(CmdDesc *d, int cmd, int idx)
{
    SeqTrack *t;
    int r = 10;
    if (!d)
        return r;
    if (cmd == 2)
        return updateChannel(d);
    switch (d->type) {
    case 0:
        return updateChannel(d);
    case 1:
        return applyTrackFlag(d);
    case 2:
        return copyChannelParams(d);
    case 3:
    case 5:
        if (cmd == 0 && idx >= 0) {
            r = listGetAt((void **)&t, idx);
            if (r == 0)
                r = t->appendIfAccepted(d);
        }
        break;
    }
    return r;
}

// MATCH: sound.dll 0x10024b80 ?scheduleEvent@Sound5b6a8@@QAEXPAUSeqTrack@@IEH@Z
void Sound5b6a8::scheduleEvent(SeqTrack *t, unsigned int tick, unsigned char mode, int ref)
{
    SeqTrack *rt;
    unsigned int len = t->m54;
    if (ref > 0 && listGetAt((void **)&rt, ref) == 0)
        len = rt->m54;
    if (len == 0)
        return;
    if (mode & 1)
        t->m50 = tick + (len - tick % len);
    else if (mode & 4)
        t->m50 = (tick / ring.d + 1) * ring.d;
    else if (mode & 0x10)
        t->m50 = tick;
    else
        t->m50 = 0;
    if (mode & 0x20)
        seekTrackToTick(t, 0);
}

// NEAR 81.4% (see re/match/wip/m36b_hand_09.cpp): sound.dll 0x100247c0 ?allocTrackSlot@Sound5b6a8@@QAEXXZ
void Sound5b6a8::allocTrackSlot()
{
    SeqTrack *t;
    if (m23c == 0)
        return;
    if (++g_trackRoundRobin >= m23c)
        g_trackRoundRobin = 0;
    if (listGetAt((void **)&t, m238[g_trackRoundRobin]) == 0) {
        int n = 0;
        while (n++ < m23c) {
            if (!(t->bits & 4)) {
                t->bits |= 4;
                break;
            }
            if (++g_trackRoundRobin >= m23c)
                g_trackRoundRobin = 0;
            if (listGetAt((void **)&t, m238[g_trackRoundRobin]) != 0)
                break;
        }
    }
    scheduleEvent(t, m71c, 0x20, 0);
}

// MATCH: sound.dll 0x100246a0 ?emitEvent@Sound5b6a8@@QAEHI@Z
int Sound5b6a8::emitEvent(unsigned int i)
{
    SeqTrack *t;
    int r = listGetAt((void **)&t, i);
    if (r == 0) {
        if (g_midiDevice->hmo) {
            for (int k = 0; k < 0x80; k++) {
                if (t->msgs[k].b[0]) {
                    t->msgs[k].b[2] = 0;
                    midiOutShortMsg(g_midiDevice->hmo, t->msgs[k].dw);
                    t->msgs[k].dw = 0;
                }
            }
            return 0;
        }
        for (int k = 0; k < 0x80; k++) {
            VoiceGroup *v = t->voices[k];
            if (v) {
                t->voices[k] = 0;
                v->stop();
                if (v->count > 1)
                    for (int j = 0; j < v->count - 1; j++)
                        v->subs[j]->stop();
            }
        }
        return 0;
    }
    return r;
}
