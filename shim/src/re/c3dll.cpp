// Batch c3dll: first reimplementations inside jgld.dll (addresses are RVAs; the module's VAs are 0x10000000 + RVA).
// jgld.dll is LoadLibrary'd after the shim starts, so these hooks install from the LoadLibraryA hook
// (SgHooksInstallPending, re/hooks.cpp), and diff_hook finds them by (module, rva) through SimGolfShim_FindHookM.
#include "hooks.h"

namespace {

// jgld's circular doubly linked list (re/match/jgld_list.cpp): ListNode {vptr; prev +4; next +8; data +0xc;
// flag +0x10}, LinkedList {vptr; m_data +4; m_flag +8; m_count +0xc; m_head +0x10; m_cur +0x14; m_ordered +0x18}.
struct JNode { void* vt; JNode* prev; JNode* next; void* data; char flag; };
struct JList { void* vt; void* data; char flag; int count; JNode* head; JNode* cur; int ordered; };

// 0x000072b0  LinkedList::find(data) (jgld.dll, VA 0x100072b0): returns 0 at once when the list has no head
// (cmp [this+0x10], 0 at 0x100072d0). Otherwise it walks from the head through each node's prev link (+4,
// 0x10007328 onward) until it is back at the head; the first node whose data (+0xc) equals the argument
// (cmp at 0x100072f5) becomes the current node (this+0x14, 0x10007300), its data and flag byte are copied to
// this+4 and this+8 (0x1000730f, 0x1000731e), and it returns 1. No match returns 0 with nothing written. The
// head's next link is read into an unused local (0x100072e0). __thiscall, one stack argument.
typedef int(__fastcall* ListFind_t)(JList*, void*, void*);
ListFind_t ListFind_orig;
int __fastcall ListFind_re(JList* self, void*, void* data) {
    if (!self->head) return 0;
    JNode* n = self->head;
    do {
        if (n->data == data) {
            self->cur = n;
            self->data = n->data;
            self->flag = n->flag;
            return 1;
        }
        n = n->prev;
    } while (n != self->head);
    return 0;
}

}  // namespace

SG_HOOK("jgld.dll", 0x000072b0, LinkedList_find, ListFind_re, ListFind_orig);
