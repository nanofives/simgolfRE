// FLAGS sound.dll: /O2 /GX
// m36c R3 (round 36): sound.dll release functions with SEH frames, 0x10037780..0x1003a900 (MultiSound ctor/dtor),
// hand-matched from Ghidra's decompilation (raw form: Ghidra's offsets and FUN_ names). Evidence of byte identity,
// not readable source.
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

// MultiSound = Sound (polymorphic, 0x70 bytes) + Channel (non-polymorphic second base at +0x70: the dtor converts
// `this` to it with a NULL test, 0x100398ff) + its own fields from +0x1420 (operator new(0x1450) in 0x100399a0).
#define MS_CLASSES(NAME) \
  struct Sound { Sound(); virtual ~Sound(); void FUN_1002ad10(int); char data[0x6c]; }; \
  struct Channel { Channel(); ~Channel(); void FUN_100340b0(void); char d[0x58]; uint flags; char d2[0x17c - 0x5c]; \
    int x17c; void setX17c(int v) { x17c = v; } char d3[0x13b0 - 0x180]; }; \
  struct NAME : public Sound, public Channel { \
    NAME(); NAME(int); ~NAME(); \
    int x1420; int x1424[2]; int x142c; void *x1430; int x1434; int count; Sound *children[4]; int x144c; \
  };
void __cdecl FUN_1004249a(void *);

namespace f_100399a0 {
struct Sound { Sound(); virtual ~Sound(); virtual int v1(); virtual int v2(); virtual int v3(); virtual int v4(uint *);
  void FUN_1002ad10(int); char data[0x6c]; };
struct Channel { Channel(); ~Channel(); int FUN_100326a0(uint *); char d[0x13b0]; };
struct C_FUN_100399a0 : public Sound, public Channel {
  // the default ctor 0x10039690, inlined here
  C_FUN_100399a0() {
    memset(x1424, 0, sizeof(x1424));
    x1420 = 0;
    x142c = 0;
    count = 1;
    children[0] = 0;
    memset(&children[1], 0, 3 * sizeof(Sound *));
    x1430 = 0;
    x1434 = 0;
    FUN_1002ad10(1);
  }
  ~C_FUN_100399a0();
  int FUN_100399a0(uint *param_1);
  int x1420; int x1424[2]; int x142c; void *x1430; int x1434; int count; Sound *children[4]; int x144c;
};
// MATCH: sound.dll 0x100399a0 ?FUN_100399a0@C_FUN_100399a0@f_100399a0@@QAEHPAI@Z
int C_FUN_100399a0::FUN_100399a0(uint *param_1)
{
  int r = FUN_100326a0(param_1);
  if ((r == 0) && (count > 1) && (count < 3)) {
    for (int i = 1; i < count; i++) {
      C_FUN_100399a0 *m = new C_FUN_100399a0;
      r = m->v4(param_1);
      if (r != 0) {
        return r;
      }
      children[i] = m;
    }
  }
  return r;
}
}

namespace f_10038710 {
struct Buf { void *data; void *del(int); };
struct Node { Node *prev; Node *next; Buf *data; };
struct List {
  Node *head; Node *tail; Node *cur; int count;
  Buf *popFront(void);
  Buf *pop(void) {
    Node *n = head;
    if (n == 0) {
      return 0;
    }
    Node *next = n->next;
    if (next != 0) {
      next->prev = 0;
      head = next;
    }
    else {
      tail = 0;
      head = 0;
    }
    Buf *d = n->data;
    FUN_1004249a(n);
    count = count - 1;
    return d;
  }
  ~List() {
    Buf *d = pop();
    while (d != 0) {
      d = pop();
    }
  }
};
struct Flags { uint b0 : 1; uint open : 1; };
struct C_FUN_10038710 {
  virtual ~C_FUN_10038710();
  int FUN_10039140(void);
  Flags flags; char pad8[0x3c]; HWAVEIN h; WAVEFORMATEX fmt; char pad5a[2]; List list;
};
// MATCH: sound.dll 0x10038710 ??1C_FUN_10038710@f_10038710@@UAE@XZ
C_FUN_10038710::~C_FUN_10038710()
{
  if (flags.open) {
    waveInStop(h);
    waveInReset(h);
    FUN_10039140();
  }
  if (flags.b0) {
    waveInStop(h);
    waveInReset(h);
  }
  // The first pop is inlined, the loop's pop is the out-of-line 0x10039620 (`call`), and each element is freed by
  // `if (b) b->??_G(1)` with the scalar deleting destructor 0x10038ae0 called, not inlined. Every `delete b` spelling
  // tried (inline / out-of-line / later-defined dtor, class operator delete) inlines ??_G, so the call is written as
  // a declared member del(int): the instructions are identical, the original spelling is not determined.
  Buf *b = list.pop();
  while (b != 0) {
    if (b != 0) {
      b->del(1);
    }
    b = list.popFront();
  }
}
}

namespace f_10039690 {
MS_CLASSES(C_FUN_10039690)
// MATCH: sound.dll 0x10039690 ??0C_FUN_10039690@f_10039690@@QAE@XZ
C_FUN_10039690::C_FUN_10039690()
{
  memset(x1424, 0, sizeof(x1424));
  x1420 = 0;
  x142c = 0;
  count = 1;
  children[0] = 0;
  memset(&children[1], 0, 3 * sizeof(Sound *));
  x1430 = 0;
  x1434 = 0;
  FUN_1002ad10(1);
}
}

namespace f_10039790 {
MS_CLASSES(C_FUN_10039790)
// MATCH: sound.dll 0x10039790 ??0C_FUN_10039790@f_10039790@@QAE@H@Z
C_FUN_10039790::C_FUN_10039790(int param_1)
{
  memset(x1424, 0, sizeof(x1424));
  x1420 = param_1;
  count = 1;
  x1430 = 0;
  x1434 = 0;
  // +0x1ec (Channel +0x17c) is written through an inline Channel setter: a direct store is scheduled after the
  // memset (90.0%).
  setX17c(param_1);
  memset(&children[1], 0, 3 * sizeof(Sound *));
  FUN_1002ad10(1);
}
}

namespace f_10039870 {
MS_CLASSES(C_FUN_10039870)
// MATCH: sound.dll 0x10039870 ??1C_FUN_10039870@f_10039870@@UAE@XZ
C_FUN_10039870::~C_FUN_10039870()
{
  for (int i = 1; i < count; i++) {
    if (children[i] != 0) {
      delete children[i];
      children[i] = 0;
    }
  }
  if ((flags & 2) != 0) {
    FUN_100340b0();
  }
  if (x1430 != 0) {
    FUN_1004249a(x1430);
    x1430 = 0;
  }
}
}
