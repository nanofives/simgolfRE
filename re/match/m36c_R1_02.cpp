// FLAGS sound.dll: /O2 /GX
// m36c R1 (round 36): sound.dll release functions with an SEH frame (C++ EH on), hand-matched from Ghidra's
// decompilation. Raw field names; evidence of byte identity, not readable source.
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_100312a0 {
// The list nodes are released with `delete` (a C++ call that may throw under /GX): with C free() VC6 drops the EH
// state-0 store before the member's destructor (92.6%).
extern int DAT_100b4a08;
struct G { void m(); };
extern G *DAT_100b4a24;
struct Node { Node *prev; Node *next; void *data; };
struct Item { HMMIO h; };
struct List {
  Node *head; Node *tail; int x; int count;
  List() { head = 0; tail = 0; x = 0; count = 0; }
  ~List() { void *p; for (p = removeHead(); p != 0; p = removeHead()) { } }
  void *removeHead() {
    Node *n = head;
    if (n == 0) return 0;
    Node *nx = n->next;
    if (nx != 0) { nx->prev = 0; head = nx; }
    else { tail = 0; head = 0; }
    void *d = n->data;
    delete n;
    count--;
    return d;
  }
};
struct Base { int b[0x16]; Base(); virtual ~Base(); };
struct C_FUN_100312a0 : Base { List list; int f6c, f70, f74, f78, f7c; virtual ~C_FUN_100312a0(); };
// MATCH: sound.dll 0x100312a0 ??1C_FUN_100312a0@f_100312a0@@UAE@XZ
C_FUN_100312a0::~C_FUN_100312a0()
{
  Item *p;
  for (p = (Item *)list.removeHead(); p != 0; p = (Item *)list.removeHead()) {
    mmioClose(p->h, 0);
    DAT_100b4a08--;
    delete p;
  }
  if (DAT_100b4a24 != 0) DAT_100b4a24->m();
}
}
