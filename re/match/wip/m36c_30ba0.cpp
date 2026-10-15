// m36c WIP (round 36): best 95.6% for sound.dll 0x10030ba0, from scratch m.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2
// scratch
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10030ba0 {
extern "C" void __cdecl free(void *);
extern int DAT_100b4a08;
extern int DAT_100b49f4;
struct Node { Node *prev; Node *next; void *data; };
struct List {
  Node *head; Node *tail; Node *cur; int count;
  void *first() { cur = head; return cur ? cur->data : 0; }
  void *removeHead() {
    Node *n = head;
    if (n == 0) return 0;
    Node *nx = n->next;
    if (nx != 0) { nx->prev = 0; head = nx; }
    else { tail = 0; head = 0; }
    void *d = n->data;
    free(n);
    count--;
    return d;
  }
  void remove(Node *n) {
    if (n == 0) return;
    if (n->prev != 0) n->prev->next = n->next; else head = n->next;
    Node *nx = n->next;
    if (nx != 0) { nx->prev = n->prev; cur = nx; }
    else { tail = n->prev; cur = 0; }
    free(n);
    count--;
  }
};
extern List DAT_100b49c8;
struct Elem { MMIOINFO info; char p0[0x9c - 0x48]; unsigned char f9c; char p1[0xcc - 0x9d]; unsigned int fcc;
  ~Elem(); };
struct Frame { int l, r; };
extern "C" int __cdecl mixA(Frame *out, HPSTR *pos, int count, Elem *e, int z);
extern "C" int __cdecl mixB(Frame *out, HPSTR *pos, int count, Elem *e, int z);
struct C_FUN_10030ba0 {
  virtual void v00(); virtual void v01(); virtual void v02(); virtual void v03();
  virtual void v04(); virtual void v05(); virtual void v06(); virtual void v07();
  virtual void v08(); virtual void v09(); virtual void v10(); virtual void v11();
  virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15();
  virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19();
  virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23();
  virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27();
  virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31();
  virtual void v32();
  char pad[0x54]; unsigned int f58; List list;
  int FUN_10030ba0(Frame *out, int count, unsigned int mode, int *written); };
// MATCH: sound.dll 0x10030ba0 ?FUN_10030ba0@C_FUN_10030ba0@f_10030ba0@@QAEHPAUFrame@2@HIPAH@Z
int C_FUN_10030ba0::FUN_10030ba0(Frame *out, int count, unsigned int mode, int *written)
{
  int stereo = 0;
  if (DAT_100b49f4 == 0) {
    return 0x13;
  }
  if (mode >= 12) {
    stereo = 1;
  }
  if (f58 & 8) {
    Elem *p;
    for (p = (Elem *)list.removeHead(); p != 0; p = (Elem *)list.removeHead()) {
      if (p->info.hmmio != 0) {
        mmioClose(p->info.hmmio, 0);
        for (Node *n = DAT_100b49c8.head; n != 0; n = n->next) {
          if (n->data == p) {
            DAT_100b49c8.remove(n);
            break;
          }
        }
      }
      DAT_100b4a08--;
      delete p;
    }
    v32();
  }
  Elem *e = (Elem *)list.first();
  if (e == 0) {
    *written = 0;
    if (stereo) {
      memset(out, 0, count * sizeof(Frame));
    }
    if (f58 & 0x20) {
      f58 &= ~1;
    }
    return 0x26;
  }
  HPSTR pos = e->info.pchNext;
  int n;
  if (stereo) {
    n = mixA(out, &pos, count, e, 0);
  }
  else {
    n = mixB(out, &pos, count, e, 0);
  }
  *written = n;
  unsigned int skip = e->fcc;
  for (unsigned int i = 0; i < skip; i++) {
    Elem *x = (Elem *)list.removeHead();
    if (x != 0) {
      mmioClose(x->info.hmmio, 0);
      DAT_100b4a08--;
      delete x;
    }
  }
  if (skip != 0) {
    e = (Elem *)list.first();
  }
  if (e != 0) {
    e->info.pchNext = pos;
    if (pos >= e->info.pchEndRead && !(e->f9c & 1)) {
      f58 = (f58 & ~0x10) | 4;
      v08();
      list.removeHead();
      mmioClose(e->info.hmmio, 0);
      DAT_100b4a08--;
      delete e;
    }
  }
  else {
    if (f58 & 0x20) {
      f58 &= ~1;
    }
  }
  if (n != count && stereo) {
    memset(&out[n], 0, (count - n) * 16);
  }
  return 0;
}
}
