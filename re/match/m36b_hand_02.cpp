// FLAGS sound.dll: /O2 /GX
// sound.dll hand matches (round 36, batch m36b): the Ctrl object family (vtables 0x1005b58c, 0x1005b590, 0x1005b19c,
// 0x1005b5c0 / 0x1005b59c). Layouts are the offsets the instructions use; class names follow re/names.
#include <string.h>

struct VObj { virtual ~VObj(); };

struct PtrNode { PtrNode *prev; PtrNode *next; void *val; };

struct PtrList {
    PtrNode *head;      // +0
    PtrNode *tail;      // +4
    PtrNode *cur;       // +8
    int count;          // +c
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
};

// vtable 0x1005b58c: an owning list of virtual objects
struct Ctrl5b58c {
    virtual ~Ctrl5b58c();
    int m4, m8;
    PtrList list;       // +c
};

// MATCH: sound.dll 0x1001c080 ??1Ctrl5b58c@@UAE@XZ
Ctrl5b58c::~Ctrl5b58c()
{
    VObj *v;
    while ((v = (VObj *)list.removeHead()) != 0)
        delete v;
}

// the same body in a second class (MidiReader_freeContent 0x1001b870 stores the same vtable 0x1005b58c)
struct MidiReaderContent {
    virtual ~MidiReaderContent();
    int m4, m8;
    PtrList list;       // +c
};

// MATCH: sound.dll 0x1001b870 ??1MidiReaderContent@@UAE@XZ
MidiReaderContent::~MidiReaderContent()
{
    VObj *v;
    while ((v = (VObj *)list.removeHead()) != 0)
        delete v;
}

struct Short0 { short v; Short0() { v = 0; } };

// vtable 0x1005b19c
struct CtrlObject {
    virtual ~CtrlObject();
    int fourcc;         // +4  'ctrl'
    int m8;             // +8
    int size;           // +c  0x18
    int tag;            // +10 'DDDD'
    int m14;            // +14 -1
    char name[4];       // +18
    int m1c;
    int m20;            // +20 -1
    int m24;            // +24 -1
    CtrlObject();
};

// MATCH: sound.dll 0x1001bc50 ??0CtrlObject@@QAE@XZ
CtrlObject::CtrlObject()
{
    m14 = -1;
    m20 = -1;
    m24 = -1;
    fourcc = 0x6c727463;
    memset(name, 0, 2);
    size = 0x18;
    tag = 0x44444444;
}
