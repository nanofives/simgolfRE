// FLAGS sound.dll: /O2 /GX
// sound.dll hand matches (round 36, batch m36b): Ctrl5b5c0 (vtables 0x1005b5c0 / 0x1005b59c), a Ctrl object with a
// 128-byte value table, built on the base CtrlObject2 (0x1005b590, owning list at +0xc) and CtrlObject (0x1005b19c)
// at +0x2c. Layouts are the offsets the instructions use. The base constructors are written in-class here so they
// inline as in the original (re/match/m36b_hand_02.cpp holds the out-of-line copies that match on their own).
#include <string.h>

struct VObj { virtual ~VObj(); };
struct PtrNode { PtrNode *prev; PtrNode *next; void *val; };

struct PtrList {
    PtrNode *head, *tail, *cur;
    int count;
    PtrList() { head = 0; tail = 0; cur = 0; count = 0; }
    ~PtrList() { for (void *v = removeHead(); v; v = removeHead()) ; }
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

struct Ctrl5b58cB {
    virtual char accept(void *p);
    ~Ctrl5b58cB()
    {
        VObj *v;
        while ((v = (VObj *)list.removeHead()) != 0)
            delete v;
    }
    int m4, m8;
    PtrList list;       // +c
    Ctrl5b58cB() { m4 = 0x18; m8 = 0x44444444; }
};

struct CtrlObject2B : Ctrl5b58cB {
    int m1c, m20, m24, m28;
    CtrlObject2B() { m28 = 0; m1c = 0xc; m20 = 0x44444444; }
};

struct CtrlObjectB {
    virtual ~CtrlObjectB() {}
    virtual void v1(); virtual void v2(); virtual void v3(); virtual int map(int v); virtual void v5();
    virtual void flush();
    int fourcc;         // +4  'ctrl'
    int m8;             // +8
    int size;           // +c  0x18
    int tag;            // +10 'DDDD'
    int m14;            // +14 -1
    char name[4];       // +18
    int m1c;
    int m20;            // +20 -1
    int m24;            // +24 -1
    CtrlObjectB()
    {
        m14 = -1;
        m20 = -1;
        m24 = -1;
        fourcc = 0x6c727463;
        memset(name, 0, 2);
        size = 0x18;
        tag = 0x44444444;
    }
};

struct CtrlElem {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual int map(int v);
    virtual void v5(); virtual void flush();
    char pad[0x15];
    char enabled;       // +19
    int type;           // +1c
};

struct Ctrl5b5c0 : CtrlObject2B, CtrlObjectB {
    struct Hdr { int size; int magic; void set(int s) { magic = 0x44444444; size = s; } } hdr;  // +54
    unsigned char table[0x80];  // +5c
    int mdc, me0;
    Ctrl5b5c0();
    Ctrl5b5c0(unsigned char a0, unsigned char a1, unsigned char a2, unsigned char a3, unsigned char a4,
              unsigned char a5, unsigned char a6, unsigned char a7, unsigned char a8, unsigned char a9,
              unsigned char a10, unsigned char a11, int a12);
    int transposeTable(unsigned int n);
    int mapValue(int v);
    void flush();
};

// MATCH: sound.dll 0x1001bd90 ??0Ctrl5b5c0@@QAE@XZ
Ctrl5b5c0::Ctrl5b5c0()
{
    for (int i = 0; i < 0x80; i++)
        table[i] = (unsigned char)i;
    CtrlObjectB::m1c = 3;
    name[0] = 1;
    me0 = 0;
    CtrlObjectB::m8 = 0xe4;
    hdr.set(0x8c);
}

// NEAR 57.0% (see re/match/wip/m36b_hand_03.cpp): sound.dll 0x1001bee0 ??0Ctrl5b5c0@@QAE@EEEEEEEEEEEEH@Z
// MATCH: sound.dll 0x1001b570 ??1Ctrl5b5c0@@UAE@XZ
Ctrl5b5c0::Ctrl5b5c0(unsigned char a0, unsigned char a1, unsigned char a2, unsigned char a3, unsigned char a4,
                     unsigned char a5, unsigned char a6, unsigned char a7, unsigned char a8, unsigned char a9,
                     unsigned char a10, unsigned char a11, int a12)
{
    table[0] = a0; table[1] = a1; table[2] = a2; table[3] = a3; table[4] = a4; table[5] = a5;
    table[6] = a6; table[7] = a7; table[8] = a8; table[9] = a9; table[10] = a10; table[11] = a11;
    transposeTable(0);
    CtrlObjectB::m1c = 3;
    me0 = 0;
    CtrlObjectB::m8 = 0xe4;
    mdc = a12;
    hdr.set(0x8c);
}

// NEAR 76.0% (see re/match/wip/m36b_hand_03.cpp): sound.dll 0x1001c370 ?mapValue@Ctrl5b5c0@@QAEHH@Z
int Ctrl5b5c0::mapValue(int v)
{
    PtrList *l = &list;
    if (l) {
        CtrlElem *e = (CtrlElem *)l->first();
        while (e) {
            switch (e->type) {
            case 3:
                v = e->map(v);
                break;
            case 4:
                v = e->map(v);
                break;
            case 5:
                if (!e->enabled)
                    return v;
                break;
            }
            e = (CtrlElem *)l->next();
        }
    }
    return table[v];
}

// MATCH: sound.dll 0x1001c2f0 ?flush@Ctrl5b5c0@@UAEXXZ
void Ctrl5b5c0::flush()
{
    for (CtrlElem *e = (CtrlElem *)list.first(); e; e = (CtrlElem *)list.next())
        e->flush();
}
