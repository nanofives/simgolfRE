// FLAGS sound.dll: /O2 /GX
// m36a (round 36) second pass, group h2: near misses with an EH frame (WIP lines, never evidence). To re-test one,
// copy it to a scratch file with its WIP line changed to MATCH and run re/tools/match.py --no-evidence.
#include <windows.h>
#include <string.h>

// Needed only so ChannelTable::loadRange is not the first function of the file (see its comment).
int dummyBeforeLoadRange(int *p) { return *p; }

namespace m36a_1aef0 {
extern int g_soundType;

struct MultiSound {
    MultiSound(int type);
    virtual ~MultiSound();
    virtual void s04(int x);
    virtual void s08();
    virtual void s0c();
    virtual int load(void *desc);
    char pad[0x1424 - 4];
    unsigned flags;
    char pad2[0x1438 - 0x1428];
    unsigned count;
    int pad3;
    MultiSound *sub[4];
};

struct RangeDesc {
    unsigned hi;
    unsigned lo;
    unsigned n;
    char data[4];
};

struct ChannelTable {
    int f0;
    MultiSound *tab[0x80];
    char pad[0x218 - 0x204];
    int f218;
    int loadRange(RangeDesc *d);
    int releaseObject(MultiSound *obj);
};

// 98.4% when another function precedes it in the file (as dummyBeforeLoadRange above). As the FIRST function of a
// file VC6 duplicates the epilogue into the error return where the original jumps to a shared one (85.0%, the
// earlier m36a_seq_wip.cpp state); any preceding function, even `int f(int *p) { return *p; }`, removes that.
// Left: the first new-expression's ctor argument goes through edx (`mov edx, [g]; mov ecx, eax; push edx`), the
// original uses ecx (`mov ecx, [g]; push ecx; mov ecx, eax`) as the loop's second new-expression does here. Not
// moved by: lo/hi order, ms declared or zero-initialised earlier, inline factory helpers, volatile or unsigned global,
// a typed copy of the global, ret/early/goto/negated exits, match_permute (380 compiles).
// WIP: sound.dll 0x1001aef0 ?loadRange@ChannelTable@m36a_1aef0@@QAEHPAURangeDesc@2@@Z 98.4
int ChannelTable::loadRange(RangeDesc *d)
{
    unsigned lo = d->lo;
    unsigned hi = d->hi;
    MultiSound *ms = new MultiSound(g_soundType);
    ms->flags |= 1;
    int ret;
    int r = ms->load(d->data);
    if (r) {
        delete ms;
        ret = r;
    } else {
    ms->s04(f218);
    for (unsigned j = 1; j < d->n; j++) {
        ms->sub[j - 1] = new MultiSound(g_soundType);
        ms->sub[j - 1]->load(d->data);
        ms->sub[j - 1]->s04(f218);
    }
    ms->count = d->n;
    unsigned i;
    for (i = lo; i <= hi; i++) {
        if (tab[i])
            i = releaseObject(tab[i]);
    }
    for (i = lo; i <= hi; i++)
        tab[i] = ms;
    ret = 0;
    }
    return ret;
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
// 83.3% (unchanged from g1). Every case loads `out` into the other of ecx/edx than the original (original: case 1
// ecx, 2 edx, 4 ecx, 5 edx, 6 ecx, 7 edx, 8 ecx; here the opposite), and in case 1 that moves `mov ecx, eax` above the
// stores. Reversing the case order in the source keeps the alternation tied to source order (case 1 stays edx).
// Not moved by: detection spellings or an inline detect helper, a copy of type, return 0 vs break, case 3 / default
// placement, MultiSound / Sound5b6a8 methods inherited from a base, reading *out back for the call, preceding
// functions in the file, volatile out (changes scheduling only), match_permute (340 compiles; its 91% needed an
// invalid reordering).
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

