// FLAGS sound.dll: /O2 /GX
// m36a group h1, functions with an EH frame. Wave_Device::readFmtChunk 0x5440 (tracker name; class Sound here):
// group g1's 97.9% version, finished. The three name-length reads reuse the fmt-size local instead of the
// chunk-length local: that puts the chunk length at esp+0x10 and the size at esp+0xc, as in the original.

#include <windows.h>
#include <string.h>

namespace m36a_5440 {
struct CtrlObject {
    int m_4;
    int m_8;
    int m_c;
    int m_10;
    int m_14;
    short m_18;
    int kind;            // 0x1c
    int m_20;
    int m_24;
    CtrlObject();
    virtual ~CtrlObject() {}
};
struct FmtObject : CtrlObject {
    FmtObject() {}
    virtual ~FmtObject() {}
};
struct CtrlHead {
    virtual ~CtrlHead();
    int m_4;
    char name8[0x18];    // 0x08
    char name20[0xc];    // 0x20
};
struct Ctrl : CtrlHead, CtrlObject {
    int m_54;
    char name58[0xe4 - 0x58];   // 0x58
    Ctrl();
};
struct Node {
    Node *prev;
    Node *next;
    CtrlObject *data;
    Node(CtrlObject *d) { prev = 0; next = 0; data = d; }
};
struct Sound {
    char pad[0xa48];
    HMMIO h;             // 0xa48
    Node *head;          // 0xa4c
    Node *tail;          // 0xa50
    Node *cur;           // 0xa54
    int count;           // 0xa58
    inline void append(CtrlObject *o)
    {
        Node *n = new Node(o);
        if (tail) {
            tail->next = n;
            n->prev = tail;
        } else {
            head = n;
        }
        tail = n;
        count++;
    }
    void readFmtChunk();
};

// MATCH: sound.dll 0x10005440 ?readFmtChunk@Sound@m36a_5440@@QAEXXZ
void Sound::readFmtChunk()
{
    FmtObject obj;
    DWORD len, size;
    mmioRead(h, (HPSTR)&len, 4);
    mmioRead(h, (HPSTR)&size, 4);
    mmioRead(h, (HPSTR)&obj.m_10, size);
    if (obj.kind == 3) {
        Ctrl *c = new Ctrl;
        CtrlObject *o = c;
        mmioRead(h, (HPSTR)&size, 4);
        mmioRead(h, c->name20, size);
        mmioRead(h, (HPSTR)&size, 4);
        mmioRead(h, c->name8, size);
        mmioRead(h, (HPSTR)&size, 4);
        mmioRead(h, c->name58, size);
        *o = obj;
        append(o);
    }

}
}
