// FLAGS sound.dll: /O2
// m36a (round 36): sound.dll sequencer / channel-table helpers 0x1ab60-0x1b160, matched by hand from the original
// instructions. Layouts are only what these functions touch.
#include <string.h>

namespace m36a_1ab60 {
struct SoundEntry {
    int f0;
    int arr[0x80];
    int f204, f208, f20c, f210;
    int f214;
    int f218;
    SoundEntry();
};

// MATCH: sound.dll 0x1001ab60 ??0SoundEntry@m36a_1ab60@@QAE@XZ
SoundEntry::SoundEntry()
{
    f0 = 0;
    for (int i = 0; i < 0x80; i++)
        arr[i] = 0;
    memset(&f214, 0, sizeof(f214));
    f204 = 0;
    f208 = 0;
    f20c = 0;
    f210 = 0;
    f218 = 250;
}
}

namespace m36a_1abd0 {
struct Obj {
    virtual ~Obj();
};
struct Seq {
    void *buf;
    Obj *ch[0x80];
    void freeChannelObjects();
};

// MATCH: sound.dll 0x1001abd0 ?freeChannelObjects@Seq@m36a_1abd0@@QAEXXZ
void Seq::freeChannelObjects()
{
    for (int i = 0; i < 0x80; i++) {
        Obj *p = ch[i];
        if (p) {
            do {
                ch[i] = 0;
                i++;
            } while (p == ch[i]);
            i--;
            delete p;
        }
    }
    if (buf)
        operator delete(buf);
    buf = 0;
}
}

namespace m36a_ms {
struct MultiSound {
    virtual ~MultiSound();
    virtual void s04(int x);
    virtual void s08();
    virtual void s0c();
    virtual int load(void *desc);
    virtual void s14();
    virtual void s18();
    virtual void s1c();
    virtual void s20();
    virtual void s24();
    virtual void s28();
    virtual void s2c();
    virtual void s30();
    virtual void s34();
    virtual void s38();
    virtual void s3c();
    virtual void s40();
    virtual void s44();
    virtual void s48();
    virtual void s4c();
    virtual void s50();
    virtual void s54();
    virtual void s58();
    virtual int isActive();
    void rewind();
    char pad[0xc8 - 4];
    unsigned char flags;
    char pad2[0x1438 - 0xc9];
    int count;
    int pad3;
    MultiSound *sub[4];
};

struct SoundMgr {
    int removeSoundNode(MultiSound *s);
    void postNotification(MultiSound *s, int code);
};
extern SoundMgr *g_soundMgr;

struct ChannelTable {
    int f0;
    MultiSound *tab[0x80];
    bool anyChannelActive();
    int notifyChannelNotes();
    int releaseObject(MultiSound *obj);
};

// MATCH: sound.dll 0x1001ad30 ?anyChannelActive@ChannelTable@m36a_ms@@QAE_NXZ
bool ChannelTable::anyChannelActive()
{
    for (int k = 0; k < 0x80; k++) {
        if (tab[k]) {
            if (tab[k]->count) {
                for (int i = 0; i < tab[k]->count - 1; i++) {
                    if (tab[k]->sub[i] && tab[k]->sub[i]->isActive())
                        return true;
                }
            }
            if (tab[k]->isActive())
                return true;
        }
    }
    return false;
}

// MATCH: sound.dll 0x1001ac40 ?notifyChannelNotes@ChannelTable@m36a_ms@@QAEHXZ
int ChannelTable::notifyChannelNotes()
{
    for (int k = 0; k < 0x80; k++) {
        if (tab[k]) {
            if (tab[k]->count) {
                for (int i = 0; i < tab[k]->count - 1; i++) {
                    if (!g_soundMgr->removeSoundNode(tab[k]->sub[i])) {
                        if (tab[k]->sub[i]->flags & 1)
                            tab[k]->sub[i]->rewind();
                        else
                            g_soundMgr->postNotification(tab[k]->sub[i], 5);
                    }
                }
            }
            if (!g_soundMgr->removeSoundNode(tab[k])) {
                if (tab[k]->flags & 1)
                    tab[k]->rewind();
                else
                    g_soundMgr->postNotification(tab[k], 5);
            }
        }
    }
    return 0;
}

// MATCH: sound.dll 0x1001b0d0 ?releaseObject@ChannelTable@m36a_ms@@QAEHPAUMultiSound@2@@Z
int ChannelTable::releaseObject(MultiSound *obj)
{
    int k;
    for (k = 0; k < 0x80; k++) {
        if (tab[k] == obj)
            tab[k] = 0;
    }
    for (int i = 0; obj->sub[i] && i < 3; i++) {
        if (obj->sub[i])
            delete obj->sub[i];
        obj->sub[i] = 0;
    }
    if (obj)
        delete obj;
    return 0;
}
}

namespace m36a_1b160 {
struct StrPtr {
    char *str;
    int assign(const char *s);
};

// MATCH: sound.dll 0x1001b160 ?assign@StrPtr@m36a_1b160@@QAEHPBD@Z
int StrPtr::assign(const char *s)
{
    if (s == 0)
        return 10;
    if (str)
        delete str;
    str = new char[strlen(s) + 1];
    strcpy(str, s);
    return 0;
}
}

namespace m36a_1adc0 {
struct Ring {
    char pad[0x204];
    unsigned end;
    unsigned start;
    unsigned cur;
    unsigned advance();
    void setPlayRange(unsigned a, unsigned b);
    char pad2[4];
    int flags;
};

// MATCH: sound.dll 0x1001adc0 ?advance@Ring@m36a_1adc0@@QAEIXZ
unsigned Ring::advance()
{
    unsigned old = cur;
    if (++cur > end)
        cur = start;
    return old;
}

// MATCH: sound.dll 0x1001ae00 ?setPlayRange@Ring@m36a_1adc0@@QAEXII@Z
void Ring::setPlayRange(unsigned a, unsigned b)
{
    if (a > b) {
        unsigned t = b;
        b = a;
        a = t;
    }
    start = a;
    flags |= 1;
    end = b;
    cur = a;
}
}

namespace m36a_1ae50 {
struct Slot {
    virtual void s0();
    virtual void s4(int x);
};
struct Chan : Slot {
    char pad[0x1438 - 4];
    int count;
    int pad2;
    Slot *sub[1];
};
struct ChannelTable {
    int f0;
    Chan *tab[0x80];
    char pad[0x218 - 0x204];
    int f218;
    void callSlot4(int x);
};

// MATCH: sound.dll 0x1001ae50 ?callSlot4@ChannelTable@m36a_1ae50@@QAEXH@Z
void ChannelTable::callSlot4(int x)
{
    Chan *prev = 0;
    for (int k = 0; k < 0x80; k++) {
        if (tab[k] && tab[k] != prev) {
            tab[k]->s4(x);
            for (int i = 0; i < tab[k]->count - 1; i++)
                tab[k]->sub[i]->s4(x);
            prev = tab[k];
        }
    }
    f218 = x;
}
}
