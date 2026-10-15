// FLAGS sound.dll: /O2 /GX
// sound.dll hand matches (round 36, batch m36b): more Ctrl objects (CtrlObject base 0x1005b19c), the name setters of
// the Sound record and Seq's list lookup. Layouts are the offsets the instructions use.
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

struct Hdr { int size; int magic; void set(int s) { magic = 0x44444444; size = s; } };

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

// a second Ctrl object on the same two bases (its destructor is 0x1001c990)
struct Ctrl5b5f0 : CtrlObject2B, CtrlObjectB {
    int m54;
    Ctrl5b5f0();
};

// the destructor is the compiler-generated one (an explicit body keeps the dead vtable stores); the constructor
// is defined only so that it is emitted
Ctrl5b5f0::Ctrl5b5f0()
{
}
// MATCH: sound.dll 0x1001c990 ??1Ctrl5b5f0@@UAE@XZ

// vtable 0x1005b644
struct CtrlObject3 : CtrlObjectB {
    int m28;
    int m2c;
    int m30, m34;
    CtrlObject3();
};

// MATCH: sound.dll 0x1001caf0 ??0CtrlObject3@@QAE@XZ
CtrlObject3::CtrlObject3()
{
    memset(&m2c, 0, 4);
    m28 = 0;
    m30 = 0;
    m34 = 0;
    m1c = 2;
    m8 = 0x38;
}

// vtable 0x1005b668
struct Ctrl5b668 : CtrlObjectB {
    int m28, m2c, m30;
    Ctrl5b668();
};

// MATCH: sound.dll 0x1001cbf0 ??0Ctrl5b668@@QAE@XZ
Ctrl5b668::Ctrl5b668()
{
    m2c = 0;
    m30 = 0;
    m1c = 1;
    m8 = 0x34;
}

struct ChanRecord {
    int m0;
    int m4;
    int m8, mc, m10, m14, m18, m1c, m20, m24;
    ChanRecord();
};

// MATCH: sound.dll 0x1001ccf0 ??0ChanRecord@@QAE@XZ
ChanRecord::ChanRecord()
{
    memset(&m4, 0, 4);
    m0 = 0;
    m8 = 0;
    mc = 0;
    m10 = 0;
    m14 = 0;
    m1c = 0;
    m20 = 0;
    m24 = 0;
    m18 = 0xfa;
}

struct Sound {
    char pad[0x1c];
    char *name;         // +1c
    char *dir;          // +20
    char *altName;      // +24
    void setName(const char *n);
    void setAltName(const char *n);
};

// MATCH: sound.dll 0x1001ced0 ?setName@Sound@@QAEXPBD@Z
void Sound::setName(const char *n)
{
    char buf[256];
    if (name)
        delete name;
    name = 0;
    if (n == 0) {
        name = 0;
        return;
    }
    if (dir) {
        strcpy(buf, dir);
        const char *p = strrchr(n, '\\');
        if (p == 0)
            p = n;
        else
            p++;
        strcat(buf, p);
    } else {
        strcpy(buf, n);
    }
    name = new char[strlen(buf) + 1];
    strcpy(name, buf);
}

// MATCH: sound.dll 0x1001d0a0 ?setAltName@Sound@@QAEXPBD@Z
void Sound::setAltName(const char *n)
{
    if (altName)
        delete altName;
    if (n) {
        altName = new char[strlen(n) + 1];
        strcpy(altName, n);
    } else {
        altName = 0;
    }
}

struct Seq {
    char pad[0x19c];
    PtrList list;       // +19c
    int listGetAt(void **out, unsigned int n);
};

// MATCH: sound.dll 0x1001d780 ?listGetAt@Seq@@QAEHPAPAXI@Z
int Seq::listGetAt(void **out, unsigned int n)
{
    unsigned int i = 0;
    *out = list.first();
    for (; i < n; i++) {
        *out = list.next();
        if (!*out)
            break;
    }
    return *out ? 0 : 0x24;
}
