// FLAGS sound.dll: /O2 /GX
// m36a (round 36): sound.dll ChannelTable_loadRange 0x1aef0 (it has an EH frame for its two new-expressions, so /GX),
// matched by hand from the original instructions.

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

// WIP: sound.dll 0x1001aef0 ?loadRange@ChannelTable@m36a_1aef0@@QAEHPAURangeDesc@2@@Z 85.0%
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
