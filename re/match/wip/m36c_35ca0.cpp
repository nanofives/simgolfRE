// m36c WIP (round 36): best 68.3% for sound.dll 0x10035ca0, from scratch r2k.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2
// scratch
#include <windows.h>
#include <stdlib.h>
typedef unsigned int uint;
typedef unsigned char uchar;

namespace f_10035ca0 {
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
};
extern List g_100b49c8;
extern unsigned short g_100b4a08x; extern int g_100b4a08;
struct R {
  char pad0[0x18]; void *buf; char pad1c[0x44 - 0x1c]; HMMIO h; int i48; int pad4c; int i50; int pad54;
  double d58; double d60; int i68; int pad6c; double d70; int i78; int pad7c; double d80; char pad88[0x94 - 0x88];
  uchar a0:1; uchar a1:7; char pad95[0x9c - 0x95];
  uchar b0:1; uchar b1:1; uchar b2:1; uchar b3:1; uchar b4:1; uchar b5:3; char pad9d[0xb4 - 0x9d];
  R *next; char padb8[0xe4 - 0xb8]; int ie4; char pade8[0xf0 - 0xe8]; double df0; double df8;
  void setPan(int p);
};
inline void closeMm(HMMIO h, void *b) { mmioClose(h, 0); free(b); }
struct C {
  char pad0[0x58]; uint flags; char pad5c[0x1c8 - 0x5c]; double dstep; char pad1d0[0x1d8 - 0x1d0]; int vol; int pad1dc;
  double dvol; int left; int pad1ec; double dleft; int right; int pad1fc; double dright; char pad208[0x224 - 0x208];
  int pan; char pad228[0x1390 - 0x228]; R *cur; int pad1394; int count;
  int closeMmio();
};
// MATCH: sound.dll 0x10035ca0 ?closeMmio@C@f_10035ca0@@QAEHXZ
int C::closeMmio()
{
  R *n = cur->next;
  void *b = cur->buf;
  mmioClose(cur->h, 0);
  free(b);
  b = (void *)(g_100b4a08 - 1);
  g_100b4a08 = (int)b;
  g_100b49c8.remove(g_100b49c8.find(cur));
  if (n) {
    if (flags & 8) {
      n->b4 = cur->b4;
      n->df8 = n->df0 = cur->df0;
      n->d60 = n->d58 = cur->d58;
      n->setPan(pan);
      if (n->d58 != 0.0 && n->df0 != 0.0) {
        *(uchar *)((char *)n + 0x9c) |= 8;
      } else {
        n->b2 = 1;
        n->a0 = 0;
        n->ie4 = 0;
      }
    }
    if (!(flags & 8)) {
      n->d58 = dvol;
      n->df0 = dstep;
      n->d80 = dleft;
      n->d70 = dright;
      n->i78 = left;
      n->i68 = right;
      n->i48 = pan;
      n->df8 = dstep;
      n->i50 = vol;
    }
  }
  if (cur) delete cur;
  cur = n;
  count--;
  return 0;
}
}
