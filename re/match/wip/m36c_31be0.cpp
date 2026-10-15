// m36c WIP (round 36): best 83.9% for sound.dll 0x10031be0, from scratch k.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2 /GX
// scratch
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10031be0 {
struct Node { Node *prev; Node *next; void *data; };
struct List {
  Node *head; Node *tail; Node *cur; int count;
  List() { head = 0; tail = 0; cur = 0; count = 0; }
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
struct Elem { char p0[0xc8]; void *owner; char p1[0x110 - 0xcc]; Elem(); ~Elem(); };
struct Bits { int a; int b; };
struct C_FUN_10031be0 {
  HMMIO h; int f4; int f8; MMIOINFO info; int f54; Bits bits; int f60; char buf[0x100];
  int f164, f168, f16c, f170, f174, f178, f17c; char desc[0x1c0 - 0x180]; int f1c0;
  char pad1[0x1d8 - 0x1c4]; int f1d8, f1dc; double d1e0; int f1e8, f1ec; double d1f0; int f1f8, f1fc; double d200;
  int f208, f20c; double d210; int f218, f21c, f220, f224, f228, f22c, f230, f234;
  Elem elem[16];
  List la; List lb; int f1358, f135c, f1360, f1364, f1368, f136c, f1370, f1374, f1378, f137c, f1380, f1384;
  double d1388; int f1390, f1394, f1398, f139c, f13a0, f13a4;
  C_FUN_10031be0(); void m1(int); };
// MATCH: sound.dll 0x10031be0 ??0C_FUN_10031be0@f_10031be0@@QAE@XZ
C_FUN_10031be0::C_FUN_10031be0()
{
  double one = 1.0;
  h = 0;
  memset(&info, 0, sizeof(MMIOINFO));
  f54 = 0;
  Bits *p = &bits;
  memset(p, 0, sizeof(Bits));
  f164 = 0;
  f168 = 0;
  f16c = 0;
  f170 = 0;
  f174 = 0;
  f17c = 0;
  f1d8 = 0x10000;
  d1e0 = one;
  f208 = 0x10000;
  d210 = one;
  f1e8 = 0x10000;
  d1f0 = one;
  f21c = 0x10000;
  f1f8 = 0x10000;
  d200 = one;
  f218 = 0x10000;
  f1c0 = 1000;
  m1(1000);
  f220 = 0x10000;
  f224 = 0;
  f228 = 0;
  f8 = 0;
  f22c = 0;
  f230 = 0;
  f1360 = 0;
  f1368 = 0;
  f136c = 0;
  memset(buf, 0, sizeof(buf));
  f1380 = 0;
  f1370 = 0;
  f1384 = 0;
  d1388 = one;
  f1358 = 0;
  f135c = 0;
  for (int i = 0; i < 16; i++) {
    elem[i].owner = &f230;
  }
  f1398 = 0;
  f1390 = 0;
  f1394 = 0;
  f139c = 0;
  f13a0 = 0;
  f13a4 = 0;
}
}
