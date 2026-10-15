// FLAGS sound.dll: /O2 /GX
// m36c R2: sound.dll Channel functions with an EH frame or C++ member objects (/GX), hand-matched from Ghidra.
#include <windows.h>
#include <stdlib.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10033c70 {
struct Node { Node *prev; Node *next; void *data; };
struct List {
  Node *head; Node *tail; Node *cur; int count;
  Node *find(void *d) {
    for (Node *n = head; n; n = n->next)
      if (n->data == d) return n;
    return 0;
  }
  void remove(Node *n) {
    if (n == 0) return;
    if (n->prev) n->prev->next = n->next;
    else head = n->next;
    Node *nx = n->next;
    if (nx) {
      nx->prev = n->prev;
      cur = nx;
    } else {
      Node *pv = n->prev;
      cur = 0;
      tail = pv;
    }
    delete n;
    count--;
  }
  void *removeHead() {
    Node *n = head;
    if (n == 0) return 0;
    Node *nx = n->next;
    if (nx) {
      nx->prev = 0;
      head = nx;
    } else {
      tail = 0;
      head = 0;
    }
    void *d = n->data;
    delete n;
    count--;
    return d;
  }
  ~List() { for (void *d = removeHead(); d != 0; d = removeHead()) ; }
};
extern List g_100b49c8;
extern int g_100b4a08;
struct V { ~V(); char pad[0x110]; };
struct Sub { char pad[0x18]; void *buf; char pad1c[0x44 - 0x1c]; HMMIO h; int pad48; };
struct C {
  HMMIO hmmio; int i04; Sub *sub; Sub emb; uint flags; char pad5c[0x238 - 0x5c];
  V v[16];
  List l1338;
  List l1348;
  char pad1358[0x13a4 - 0x1358]; int i13a4;
  ~C();
};
// MATCH: sound.dll 0x10033c70 ??1C@f_10033c70@@QAE@XZ
C::~C()
{
  if (i13a4) i13a4 = 0;
  if ((flags & 2) && hmmio) {
    mmioClose(hmmio, 0);
    g_100b49c8.remove(g_100b49c8.find(&emb));
    g_100b4a08--;
    flags &= ~2;
  }
  if (sub && sub != &emb) {
    void *b = sub->buf;
    mmioClose(sub->h, 0);
    delete sub;
    free(b);
    sub = 0;
  }
}
}
