// FLAGS sound.dll: /O2
// m36c R1 (round 36): sound.dll release functions 0x10030000..0x10033c00 hand-matched from Ghidra's decompilation.
// Raw field names (fNNN = offset); evidence of byte identity, not readable source.
#include <windows.h>
#include <mmreg.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_100303f0 {
extern "C" void __cdecl free(void *);
extern int DAT_100b4a08;
struct Node { Node *prev; Node *next; void *data; };
struct List {
  Node *head; Node *tail; Node *cur; int count;
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
struct Item { MMIOINFO info; ~Item(); };
struct C_FUN_100303f0 {
  virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3();
  virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7();
  virtual int stopDevice();
  char pad[0x40]; unsigned char f44; char pad2[0x13]; unsigned int f58; List list;
  int FUN_100303f0(); };
// MATCH: sound.dll 0x100303f0 ?FUN_100303f0@C_FUN_100303f0@f_100303f0@@QAEHXZ
int C_FUN_100303f0::FUN_100303f0()
{
  Item *p;
  if (f44 & 2) {
    stopDevice();
    f58 |= 8;
    return 0;
  }
  for (p = (Item *)list.removeHead(); p != 0; p = (Item *)list.removeHead()) {
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
  return 0;
}
}

namespace f_10030570 {
extern "C" void __cdecl free(void *);
extern int DAT_100b4a08;
struct Node { Node *prev; Node *next; void *data; };
struct List {
  Node *head; Node *tail; Node *cur; int count;
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
struct Item { MMIOINFO info; ~Item(); };
struct C_FUN_10030570 { char pad[0x5c]; List list; int FUN_10030570(); };
// MATCH: sound.dll 0x10030570 ?FUN_10030570@C_FUN_10030570@f_10030570@@QAEHXZ
int C_FUN_10030570::FUN_10030570()
{
  Item *p;
  for (p = (Item *)list.removeHead(); p != 0; p = (Item *)list.removeHead()) {
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
  return 0;
}
}

namespace f_10030710 {
struct C_FUN_10030710 { int FUN_10030710(int param_1); int callee(int); };
// MATCH: sound.dll 0x10030710 ?FUN_10030710@C_FUN_10030710@f_10030710@@QAEHH@Z
int C_FUN_10030710::FUN_10030710(int param_1)
{
  return callee(param_1);
}
}

namespace f_10030fb0 {
extern WAVEFORMATEX DAT_10063ea0;
int __stdcall FUN_1003aea0();
struct C_FUN_10030fb0 { char pad[0xb4]; int fb4; int fb8; WAVEFORMATEX wfx; char pad2[6]; int fd4;
  int FUN_10030fb0(int param_1, WAVEFORMATEX *param_2, int param_3); };
// MATCH: sound.dll 0x10030fb0 ?FUN_10030fb0@C_FUN_10030fb0@f_10030fb0@@QAEHHPAUtWAVEFORMATEX@@H@Z
int C_FUN_10030fb0::FUN_10030fb0(int param_1, WAVEFORMATEX *param_2, int param_3)
{
  if (param_1 == 0) {
    return 10;
  }
  if (param_2 != 0) {
    wfx = *param_2;
  }
  else {
    wfx = DAT_10063ea0;
  }
  fb4 = param_1;
  fd4 = FUN_1003aea0();
  return 0;
}
}

namespace f_10031200 {
struct Base { int b[0x16]; Base(); virtual void v0(); };
struct R { int a; };
struct List4 { int h, t, c, d; List4() { h = 0; t = 0; c = 0; d = 0; } };
struct C_FUN_10031200 : Base { List4 f5c; int f6c, f70, f74, f78; R f7c; int f80, f84, f88, f8c, f90;
  C_FUN_10031200(); virtual void v0(); };
// MATCH: sound.dll 0x10031200 ??0C_FUN_10031200@f_10031200@@QAE@XZ
C_FUN_10031200::C_FUN_10031200()
{
  f70 = 0;
  memset(&f7c, 0, sizeof(f7c));
  f74 = 0;
  f90 = 0;
  f84 = 0;
  f88 = 0;
  f80 = 0;
  f8c = 0;
}
}

namespace f_10031450 {
struct G { int start(void *); };
extern G *DAT_100b4a24;
struct C_FUN_10031450 { int FUN_10031450(int param_1, int param_2, int param_3); };
// MATCH: sound.dll 0x10031450 ?FUN_10031450@C_FUN_10031450@f_10031450@@QAEHHHH@Z
int C_FUN_10031450::FUN_10031450(int param_1, int param_2, int param_3)
{
  *(int *)((int)this + 0x6c) = param_1;
  if (DAT_100b4a24 == 0) {
    return 0x13;
  }
  return DAT_100b4a24->start(this);
}
}

namespace f_10031490 {
extern int DAT_100b4a08;
extern "C" void __cdecl free(void *);
struct Node { Node *prev; Node *next; void *data; };
struct Item { HMMIO h; };
struct List {
  Node *head; Node *tail; int x; int count;
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
};
struct C_FUN_10031490 { int pad[0x17]; List list; int f6c, f70, f74; int FUN_10031490(); };
// MATCH: sound.dll 0x10031490 ?FUN_10031490@C_FUN_10031490@f_10031490@@QAEHXZ
int C_FUN_10031490::FUN_10031490()
{
  Item *p;
  for (p = (Item *)list.removeHead(); p != 0; p = (Item *)list.removeHead()) {
    mmioClose(p->h, 0);
    DAT_100b4a08--;
    free(p);
  }
  f74 = 0;
  return 0;
}
}

namespace f_10031560 {
extern int DAT_100b4a08;
extern "C" void __cdecl free(void *);
struct G { void m(); };
extern G *DAT_100b4a24;
struct Node { Node *prev; Node *next; void *data; };
struct Item { HMMIO h; };
struct List {
  Node *head; Node *tail; int x; int count;
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
};
struct C_FUN_10031560 { int pad[0x17]; List list; int f6c, f70, f74, f78; unsigned int f7c; int FUN_10031560(); };
// MATCH: sound.dll 0x10031560 ?FUN_10031560@C_FUN_10031560@f_10031560@@QAEHXZ
int C_FUN_10031560::FUN_10031560()
{
  Item *p;
  MMIOINFO info;
  for (p = (Item *)list.removeHead(); p != 0; p = (Item *)list.removeHead()) {
    mmioGetInfo(p->h, &info, 0);
    mmioClose(p->h, 0);
    DAT_100b4a08--;
    free(p);
  }
  if (DAT_100b4a24 != 0) DAT_100b4a24->m();
  f7c &= ~4;
  return 0;
}
}

namespace f_100319e0 {
struct G { int start(void *); };
extern G *DAT_100b4a24;
int __stdcall FUN_1003aa90();
struct C_FUN_100319e0 { int FUN_100319e0(); };
// MATCH: sound.dll 0x100319e0 ?FUN_100319e0@C_FUN_100319e0@f_100319e0@@QAEHXZ
int C_FUN_100319e0::FUN_100319e0()
{
  *(int *)((int)this + 0x80) = 0;
  *(int *)((int)this + 0x84) = 0;
  *(int *)((int)this + 0x8c) = *(int *)((int)this + 0x8c) + 1;
  *(int *)((int)this + 0x88) = 0;
  *(int *)((int)this + 0x90) = FUN_1003aa90();
  DAT_100b4a24->start(this);
  return 0;
}
}

namespace f_10031ac0 {
extern "C" void __cdecl free(void *);
struct Node { Node *prev; Node *next; void *data; };
struct C_FUN_10031ac0 {
  Node *head; Node *tail; int x; int count;
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
  void FUN_10031ac0();
};
// MATCH: sound.dll 0x10031ac0 ?FUN_10031ac0@C_FUN_10031ac0@f_10031ac0@@QAEXXZ
void C_FUN_10031ac0::FUN_10031ac0()
{
  void *p;
  for (p = removeHead(); p != 0; p = removeHead()) {
  }
}
}

namespace f_10032020 {
extern "C" void __cdecl free(void *);
extern int DAT_100b4a08;
struct Node { Node *prev; Node *next; void *data; };
struct List {
  Node *head; Node *tail; Node *cur; int count;
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
struct Bits { unsigned int b0 : 1; unsigned int b1 : 1; unsigned int rest : 30; int f5c; };
struct C_FUN_10032020 { HMMIO h; int f4; void *buf; MMIOINFO info; int f54; Bits bits; void FUN_10032020(); };
// MATCH: sound.dll 0x10032020 ?FUN_10032020@C_FUN_10032020@f_10032020@@QAEXXZ
void C_FUN_10032020::FUN_10032020()
{
  if (buf != 0) {
    free(buf);
    buf = 0;
  }
  memset(&info, 0, sizeof(MMIOINFO));
  Bits *p = &bits;
  memset(p, 0, sizeof(Bits));
  p->b0 = 0;
  p->b1 = 0;
  if (h != 0) {
    mmioClose(h, 0);
    for (Node *n = DAT_100b49c8.head; n != 0; n = n->next) {
      if (n->data == &info) {
        DAT_100b49c8.remove(n);
        break;
      }
    }
    DAT_100b4a08--;
  }
  h = 0;
}
}

namespace f_10032130 {
struct Buf { char pad[0x20]; void *owner;
  int lock(int off, int size, void **p1, int *n1, void **p2, int *n2);
  int unlock(void *p1, int n1, void *p2, int n2); };
struct Dev { int create(Buf **out, void *desc, int flags, int size); int release(Buf *b); };
struct C_FUN_10032130 { HMMIO h; char pad[0x50]; Buf *buf; unsigned int f58; char pad2[0x178 - 0x5c]; int size;
  int f17c; char desc[0x220 - 0x180]; int f220;
  int FUN_10032130(Dev *dev); };
// MATCH: sound.dll 0x10032130 ?FUN_10032130@C_FUN_10032130@f_10032130@@QAEHPAUDev@2@@Z
int C_FUN_10032130::FUN_10032130(Dev *dev)
{
  void *p1;
  int n1;
  void *p2;
  int n2;
  if (dev != 0) {
    if (dev->create(&buf, desc, 0x100e0, size) == 0) {
      f58 |= 0x10000000;
      if (buf->lock(0, size, &p1, &n1, &p2, &n2) == 0 && n1 == size) {
        if (mmioRead(h, (HPSTR)p1, n1) != n1) {
          buf->unlock(p1, n1, p2, n2);
          dev->release(buf);
          return 0x23;
        }
        buf->unlock(p1, n1, p2, n2);
        buf->owner = this;
      }
    }
    else {
      return 0x11;
    }
  }
  f220 = 0x5622;
  return 0;
}
}

namespace f_10032290 {
struct Buf { char pad[0x20]; void *owner;
  int lock(int off, int size, void **p1, int *n1, void **p2, int *n2);
  int unlock(void *p1, int n1, void *p2, int n2); };
struct Dev { char pad[0x6c]; unsigned char f6c; int create(Buf **out, void *desc, int flags, int size); int release(Buf *b); };
struct C_FUN_10032290 { HMMIO h; char pad[0x50]; Buf *buf; unsigned int f58; char pad2[0x178 - 0x5c]; int size;
  int f17c; char desc[0x220 - 0x180]; int f220;
  int FUN_10032290(Dev *dev); };
// MATCH: sound.dll 0x10032290 ?FUN_10032290@C_FUN_10032290@f_10032290@@QAEHPAUDev@2@@Z
int C_FUN_10032290::FUN_10032290(Dev *dev)
{
  void *p1;
  int n1;
  void *p2;
  int n2;
  if (dev != 0) {
    if ((dev->f6c & 8) == 0) {
      return 0xb;
    }
    if (dev->create(&buf, desc, 0x100f4, size) == 0) {
      f58 |= 0x10000000;
      if (buf->lock(0, size, &p1, &n1, &p2, &n2) == 0 && n1 == size) {
        if (mmioRead(h, (HPSTR)p1, n1) != n1) {
          buf->unlock(p1, n1, p2, n2);
          dev->release(buf);
          return 0x23;
        }
        buf->unlock(p1, n1, p2, n2);
        buf->owner = this;
      }
    }
    else {
      return 0x11;
    }
  }
  else {
    return 1;
  }
  f220 = 0x5622;
  return 0;
}
}

namespace f_10032440 {
extern "C" void __cdecl free(void *);
struct Node { Node *prev; Node *next; void *data; };
struct List {
  Node *head; Node *tail; Node *cur; int count;
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
struct Voice { int pad[8]; int f20; };
struct C_FUN_10032440 { char pad[0x1338]; List list; void FUN_10032440(Voice *v); };
// MATCH: sound.dll 0x10032440 ?FUN_10032440@C_FUN_10032440@f_10032440@@QAEXPAUVoice@2@@Z
void C_FUN_10032440::FUN_10032440(Voice *v)
{
  if (v == 0) return;
  v->f20 = 0;
  for (Node *n = list.head; n != 0; n = n->next) {
    if (n->data == v) {
      list.remove(n);
      return;
    }
  }
}
}

namespace f_10032500 {
struct Dev {
  virtual void v00(); virtual void v01(); virtual void v02(); virtual void v03();
  virtual void v04(); virtual void v05(); virtual void v06(); virtual void v07();
  virtual void v08(); virtual void v09(); virtual void v10(); virtual void v11();
  virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15();
  virtual unsigned int rate();
};
extern Dev *DAT_100b4a20;
struct Voice { char pad[0x50]; int f50; int f54; double f58; char pad2[0x94 - 0x60]; unsigned char f94; char pad3[7];
  unsigned char f9c; char pad4[0xe4 - 0x9d]; int fe4; int fe8; int fec; double ff0; double ff8; };
struct C_FUN_10032500 { char pad[0x1c0]; unsigned int f1c0; char pad2[0x1390 - 0x1c4]; Voice *v; int FUN_10032500(); };
// MATCH: sound.dll 0x10032500 ?FUN_10032500@C_FUN_10032500@f_10032500@@QAEHXZ
int C_FUN_10032500::FUN_10032500()
{
  Voice *p = v;
  if (p != 0) {
    unsigned int r = f1c0;
    if (DAT_100b4a20 != 0) {
      double fd = DAT_100b4a20->rate();
      double fr = r;
      double x;
      if (fr > fd) {
        double ratio = fr / fd;
        p->fec = (int)(p->f50 / ratio);
        x = p->f58 / ratio;
      }
      else {
        p->fec = p->f50;
        x = p->f58;
      }
      p->ff0 = x;
      p->ff8 = x;
    }
    p = v;
    if (p->f58 != 0.0 && p->ff0 != 0.0) {
      p->f9c |= 8;
      return 0;
    }
    p->f9c |= 4;
    p->f94 &= ~1;
    p->fe4 = 0;
  }
  return 0;
}
}

namespace f_10033ba0 {
struct Bits { int b0 : 1; int b1 : 1; int b2 : 1; int b3 : 26; int b29 : 1; int b30 : 1; int b31 : 1; };
struct S { Bits bits; int f5c; };
struct C_FUN_10033ba0 { char pad[0x58]; S s; char pad2[0x1d8 - 0x60]; int f1d8, f1dc, f1e0, f1e4;
  char pad3[0x208 - 0x1e8]; int f208, f20c, f210, f214; char pad4[0x224 - 0x218]; int f224;
  void FUN_10033ba0(); void callee(int); };
// MATCH: sound.dll 0x10033ba0 ?FUN_10033ba0@C_FUN_10033ba0@f_10033ba0@@QAEXXZ
void C_FUN_10033ba0::FUN_10033ba0()
{
  S *p = &s;
  int s31 = p->bits.b31;
  int s2 = p->bits.b2;
  int s0 = p->bits.b0;
  int s29 = p->bits.b29;
  memset(p, 0, sizeof(S));
  p->bits.b0 = s0;
  p->bits.b2 = s2;
  p->bits.b31 = s31;
  p->bits.b29 = s29;
  f1d8 = f208;
  f1e0 = f210;
  f1e4 = f214;
  callee(f224);
}
}
