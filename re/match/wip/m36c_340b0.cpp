// m36c WIP (round 36): best 97.8% for sound.dll 0x100340b0, from scratch r2i.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2 /GX
// scratch
#include <windows.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned int uint;

namespace f_100340b0 {
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
};
struct Mgr { void release(void *p); };
extern List g_100b49c8;
extern int g_100b4a08;
extern Mgr *g_100b49f0;
struct V { char pad[0x110]; };
struct Sub { char pad[0x18]; void *buf; char pad1c[0x44 - 0x1c]; HMMIO h; };
struct C {
  HMMIO hmmio; HMMIO h2; Sub *sub; Sub emb; void *dev; uint flags; uint flags5c; char pad60[0x168 - 0x60];
  int i168; int pad16c; int i170; int i174; char pad178[0x1b8 - 0x178]; void *buf1b8; char pad1bc[0x238 - 0x1bc];
  V v[16];
  List l1338;
  List l1348;
  void f_10033ba0();
  int releaseBuffers();
};
// MATCH: sound.dll 0x100340b0 ?releaseBuffers@C@f_100340b0@@QAEHXZ
int C::releaseBuffers()
{
  if (flags & 2) {
    if (flags5c & 4) return 0xb;
    if (dev) {
      Mgr *mgr = g_100b49f0;
      void *d;
      while ((d = l1338.removeHead()) != 0)
        mgr->release(d);
      mgr->release(dev);
      dev = 0;
      flags &= ~2;
      return 0;
    }
    mmioClose(hmmio, 0);
    g_100b49c8.remove(g_100b49c8.find(&emb));
    g_100b4a08--;
    memset(&emb, 0, sizeof(emb));
    if (!(flags & 1)) {
      Sub *s = sub;
      mmioClose(h2, 0);
      void *b = s->buf;
      g_100b49c8.remove(g_100b49c8.find(&emb));
      g_100b4a08--;
      free(b);
      delete sub;
      sub = 0;
    }
    if (buf1b8) {
      delete buf1b8;
      buf1b8 = 0;
    }
    for (void *d = l1348.removeHead(); d != 0; d = l1348.removeHead())
      operator delete(d);
    f_10033ba0();
  }
  hmmio = 0;
  h2 = 0;
  i168 = 0;
  i174 = 0;
  i170 = 0;
  memset(v, 0, sizeof(v));
  sub = 0;
  return 0;
}
}
