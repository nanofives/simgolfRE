// FLAGS sound.dll: /O2 /GX
// sound.dll hand matches (round 36, batch m36b): the global sound table (a PtrList at 0x100b4fe8: head, tail
// 0x100b4fec, cursor 0x100b4ff0, count 0x100b4ff4) and the functions that walk it.
#include <string.h>

struct PtrNode { PtrNode *prev; PtrNode *next; void *val; };

struct PtrListG {
    PtrNode *head, *tail, *cur;
    int count;
    void *first() { cur = head; return cur ? cur->val : 0; }
    void *next() { if (!cur) return 0; cur = cur->next; return cur ? cur->val : 0; }
    void *current() { return cur ? cur->val : 0; }
    void removeCurrent()
    {
        PtrNode *n = cur;
        if (!n)
            return;
        if (n->prev)
            n->prev->next = n->next;
        else
            head = n->next;
        PtrNode *nx = n->next;
        if (nx) {
            nx->prev = n->prev;
            cur = nx;
        } else {
            tail = n->prev;
            cur = 0;
        }
        delete n;
        count--;
    }
};
extern PtrListG g_cmdTable;         // 0x100b4fe8

struct SoundObj {
    char *name;                     // +0
    char pad[0x20c];
    int refs;                       // +210
    bool anyChannelActive();
    ~SoundObj();                    // Seq_freeChannelObjects
};

// MATCH: sound.dll 0x100204e0 ?Sound_reapIdle@@YAXXZ
void Sound_reapIdle()
{
    SoundObj *s = (SoundObj *)g_cmdTable.first();
    while (s) {
        if (s->refs > 0 || s->anyChannelActive()) {
            s = (SoundObj *)g_cmdTable.next();
        } else {
            g_cmdTable.removeCurrent();
            delete s;
            s = (SoundObj *)g_cmdTable.current();
        }
    }
}

// MATCH: sound.dll 0x10021240 ?Sound_findByName@@YG_NPBD@Z
bool __stdcall Sound_findByName(const char *name)
{
    for (SoundObj *s = (SoundObj *)g_cmdTable.first(); s; s = (SoundObj *)g_cmdTable.next())
        if (strcmp(s->name, name) == 0)
            return true;
    return false;
}
